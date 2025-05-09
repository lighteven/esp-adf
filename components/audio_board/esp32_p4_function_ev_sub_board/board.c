/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2025 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "esp_log.h"
#include "board.h"
#include "audio_mem.h"
#include "periph_sdcard.h"
#include "periph_adc_button.h"

#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "sd_pwr_ctrl_interface.h"
#include "esp_ldo_regulator.h"

#define LDO_CHANNEL_ID      (4)
#define LDO_CFG_VOL_MV      (3300)

static const char *TAG = "AUDIO_BOARD";

static audio_board_handle_t     board_handle    = 0;
static esp_ldo_channel_handle_t ldo_audio_board = NULL;

static void _enable_audio_board_power(void)
{
    // Turn on the power for audio board, so it can go from "No Power" state to "Shutdown" state
    esp_ldo_channel_config_t ldo_audio_board_config = {
        .chan_id = LDO_CHANNEL_ID,
        .voltage_mv = LDO_CFG_VOL_MV,
    };
    esp_ldo_acquire_channel(&ldo_audio_board_config, &ldo_audio_board);
    ESP_LOGI(TAG, "Audio board powered on");
}

static void _disable_audio_board_power(void)
{
    if (ldo_audio_board) {
        esp_ldo_release_channel(ldo_audio_board);
        ldo_audio_board = NULL;
    }
}

audio_board_handle_t audio_board_init(void)
{
    if (board_handle) {
        ESP_LOGW(TAG, "The board has already been initialized!");
        return board_handle;
    }
    board_handle = (audio_board_handle_t)audio_calloc(1, sizeof(struct audio_board_handle));
    AUDIO_MEM_CHECK(TAG, board_handle, return NULL);
    board_handle->audio_hal = audio_board_codec_init();
    board_handle->adc_hal = audio_board_adc_init();
    _enable_audio_board_power();
    return board_handle;
}

audio_hal_handle_t audio_board_adc_init(void)
{
    audio_hal_codec_config_t audio_codec_cfg = AUDIO_CODEC_DEFAULT_CONFIG();
    audio_codec_cfg.codec_mode = AUDIO_HAL_CODEC_MODE_ENCODE;
    audio_hal_handle_t adc_hal = NULL;
    adc_hal = audio_hal_init(&audio_codec_cfg, &AUDIO_CODEC_ES7210_DEFAULT_HANDLE);
    AUDIO_NULL_CHECK(TAG, adc_hal, return NULL);
    return adc_hal;
}

audio_hal_handle_t audio_board_codec_init(void)
{
    audio_hal_codec_config_t audio_codec_cfg = AUDIO_CODEC_DEFAULT_CONFIG();
    audio_hal_handle_t codec_hal = audio_hal_init(&audio_codec_cfg, &AUDIO_CODEC_ES8311_DEFAULT_HANDLE);
    AUDIO_NULL_CHECK(TAG, codec_hal, return NULL);
    return codec_hal;
}

esp_err_t audio_board_sdcard_init(esp_periph_set_handle_t set, periph_sdcard_mode_t mode)
{
    periph_sdcard_cfg_t sdcard_cfg = {
        .root = "/sdcard",
        .card_detect_pin = get_sdcard_intr_gpio(),
        .mode = mode
    };
    esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);
    esp_err_t ret = esp_periph_start(set, sdcard_handle);
    int retry_time = 5;
    bool mount_flag = false;
    while (retry_time--) {
        if (periph_sdcard_is_mounted(sdcard_handle)) {
            mount_flag = true;
            break;
        } else {
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    }
    if (mount_flag == false) {
        ESP_LOGE(TAG, "Sdcard mount failed");
        return ESP_FAIL;
    }
    return ret;
}

esp_err_t audio_board_key_init(esp_periph_set_handle_t set)
{
    ESP_LOGE(TAG, "esp32_p4_function_ev_board not support key");
    return ESP_FAIL;
}

audio_board_handle_t audio_board_get_handle(void)
{
    return board_handle;
}

esp_err_t audio_board_deinit(audio_board_handle_t audio_board)
{
    esp_err_t ret = ESP_OK;
    ret |= audio_hal_deinit(audio_board->audio_hal);
    audio_free(audio_board);
    _disable_audio_board_power();
    board_handle = NULL;
    return ret;
}
