/**
 * @file mochi_asset_play.c
 * @brief Asset playback implementation for MochiState
 *
 * Handles playing sounds and displaying images from either
 * embedded flash or SD card storage.
 */

#include "mochi_assets.h"
#include "audio_driver.h"
#include "bsp_board.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "mochi_asset";

/*===========================================================================
 * Sound Asset Playback
 *===========================================================================*/

esp_err_t mochi_play_asset_sound(const mochi_sound_asset_t *asset, bool loop)
{
    if (!asset || asset->source == MOCHI_ASSET_NONE) {
        return ESP_OK;  /* Nothing to play */
    }

    if (asset->source == MOCHI_ASSET_EMBEDDED) {
        /* Play embedded PCM data */
        if (!asset->embedded.pcm_data || asset->embedded.pcm_len == 0) {
            ESP_LOGW(TAG, "Embedded sound has no data (pcm_data=%p, len=%zu)",
                     (void*)asset->embedded.pcm_data, asset->embedded.pcm_len);
            return ESP_ERR_INVALID_ARG;
        }

        ESP_LOGI(TAG, "Playing embedded PCM: data=%p, len=%zu, rate=%lu, ch=%d",
                 (void*)asset->embedded.pcm_data,
                 asset->embedded.pcm_len,
                 (unsigned long)asset->embedded.sample_rate,
                 asset->embedded.channels);

        esp_err_t ret = Audio_Play_PCM(asset->embedded.pcm_data,
                                       asset->embedded.pcm_len,
                                       asset->embedded.sample_rate,
                                       asset->embedded.channels,
                                       loop);
        ESP_LOGI(TAG, "Audio_Play_PCM returned: %d (%s)", ret, esp_err_to_name(ret));
        return ret;
    }
    else if (asset->source == MOCHI_ASSET_SDCARD) {
        /* Play from SD card */
        if (!asset->sd_path) {
            ESP_LOGW(TAG, "SD sound has no path");
            return ESP_ERR_INVALID_ARG;
        }

        /* Build full path */
        char full_path[128];
        if (asset->sd_path[0] == '/') {
            /* Already absolute path */
            snprintf(full_path, sizeof(full_path), "file:/%s", asset->sd_path);
        } else {
            /* Relative to Sounds folder */
            snprintf(full_path, sizeof(full_path), "file:/" MOCHI_SD_SOUNDS_PATH "%s", asset->sd_path);
        }

        ESP_LOGI(TAG, "Playing SD sound: %s", full_path);
        return Audio_Play_Music(full_path);
    }

    return ESP_ERR_INVALID_ARG;
}

void mochi_stop_asset_sound(void)
{
    Audio_Stop_Play();
}

/*===========================================================================
 * Image Asset Display
 *===========================================================================*/

lv_obj_t* mochi_create_asset_image(lv_obj_t *parent, const mochi_image_asset_t *asset)
{
    if (!parent || !asset || asset->source == MOCHI_ASSET_NONE) {
        return NULL;
    }

    lv_obj_t *img = lv_image_create(parent);
    if (!img) {
        ESP_LOGE(TAG, "Failed to create image object");
        return NULL;
    }

    if (asset->source == MOCHI_ASSET_EMBEDDED) {
        if (!asset->embedded) {
            ESP_LOGW(TAG, "Embedded image has no descriptor");
            lv_obj_del(img);
            return NULL;
        }
        lv_image_set_src(img, asset->embedded);
        ESP_LOGD(TAG, "Created embedded image");
    }
    else if (asset->source == MOCHI_ASSET_SDCARD) {
        if (!asset->sd_path) {
            ESP_LOGW(TAG, "SD image has no path");
            lv_obj_del(img);
            return NULL;
        }

        /* Build full path for LVGL file system */
        static char img_path[128];  /* Static to persist for LVGL */
        if (asset->sd_path[0] == '/') {
            snprintf(img_path, sizeof(img_path), "S:%s", asset->sd_path);
        } else {
            snprintf(img_path, sizeof(img_path), "S:" MOCHI_SD_IMAGES_PATH "%s", asset->sd_path);
        }

        lv_image_set_src(img, img_path);
        ESP_LOGI(TAG, "Created SD image: %s", img_path);
    }

    return img;
}

esp_err_t mochi_update_asset_image(lv_obj_t *img, const mochi_image_asset_t *asset)
{
    if (!img) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!asset || asset->source == MOCHI_ASSET_NONE) {
        lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
        return ESP_OK;
    }

    lv_obj_clear_flag(img, LV_OBJ_FLAG_HIDDEN);

    if (asset->source == MOCHI_ASSET_EMBEDDED) {
        if (!asset->embedded) {
            return ESP_ERR_INVALID_ARG;
        }
        lv_image_set_src(img, asset->embedded);
    }
    else if (asset->source == MOCHI_ASSET_SDCARD) {
        if (!asset->sd_path) {
            return ESP_ERR_INVALID_ARG;
        }

        static char img_path[128];
        if (asset->sd_path[0] == '/') {
            snprintf(img_path, sizeof(img_path), "S:%s", asset->sd_path);
        } else {
            snprintf(img_path, sizeof(img_path), "S:" MOCHI_SD_IMAGES_PATH "%s", asset->sd_path);
        }
        lv_image_set_src(img, img_path);
    }

    return ESP_OK;
}
