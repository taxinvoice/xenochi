/**
 * @file mochi_asset_setup.c
 * @brief Configure MochiState assets at runtime
 *
 * This file configures MochiStates with embedded assets from main/assets/.
 * Must be called after mochi_init() and before mochi_create().
 */

#include "mochi_state.h"
#include "mochi_assets.h"
#include "assets.h"
#include "esp_log.h"

static const char *TAG = "mochi_setup";

/**
 * @brief Configure MochiStates with embedded assets
 *
 * Call after mochi_init(), before mochi_create().
 *
 * Example usage in main.cpp:
 *   mochi_init();
 *   mochi_setup_assets();  // Add embedded sounds/images
 *   mochi_create(parent);
 */
void mochi_setup_assets(void)
{
    ESP_LOGI(TAG, ">>> mochi_setup_assets() CALLED <<<");
    ESP_LOGI(TAG, "beep_8k_mono addr=%p, len=%zu", (void*)beep_8k_mono, beep_8k_mono_len);

    /* Get current HAPPY config and add embedded sound */
    const mochi_state_config_t *happy_base = mochi_get_state_config(MOCHI_STATE_HAPPY);
    ESP_LOGI(TAG, "happy_base=%p", (void*)happy_base);

    if (happy_base) {
        mochi_state_config_t happy_cfg = *happy_base;

        /* Enter sound - plays once when entering HAPPY state */
        happy_cfg.audio.enter.source = MOCHI_ASSET_EMBEDDED;
        happy_cfg.audio.enter.embedded.pcm_data = beep_8k_mono;
        happy_cfg.audio.enter.embedded.pcm_len = beep_8k_mono_len;
        happy_cfg.audio.enter.embedded.sample_rate = 8000;
        happy_cfg.audio.enter.embedded.channels = 1;

        /* Loop sound - plays continuously while in HAPPY state
         * Note: Embedded PCM loop is synchronous (blocks).
         * For non-blocking loop, use SD card file instead. */
        happy_cfg.audio.loop.source = MOCHI_ASSET_EMBEDDED;
        happy_cfg.audio.loop.embedded.pcm_data = beep_8k_mono;
        happy_cfg.audio.loop.embedded.pcm_len = beep_8k_mono_len;
        happy_cfg.audio.loop.embedded.sample_rate = 8000;
        happy_cfg.audio.loop.embedded.channels = 1;

        ESP_LOGI(TAG, "Configuring HAPPY enter: source=%d, pcm=%p, len=%zu",
                 happy_cfg.audio.enter.source,
                 (void*)happy_cfg.audio.enter.embedded.pcm_data,
                 happy_cfg.audio.enter.embedded.pcm_len);
        ESP_LOGI(TAG, "Configuring HAPPY loop: source=%d, pcm=%p, len=%zu",
                 happy_cfg.audio.loop.source,
                 (void*)happy_cfg.audio.loop.embedded.pcm_data,
                 happy_cfg.audio.loop.embedded.pcm_len);

        mochi_configure_state(MOCHI_STATE_HAPPY, &happy_cfg);
        ESP_LOGI(TAG, "HAPPY state configured with enter + loop beep");
    } else {
        ESP_LOGE(TAG, "Failed to get HAPPY base config!");
    }

    /* Get current SLEEPY config and add embedded background */
    const mochi_state_config_t *sleepy_base = mochi_get_state_config(MOCHI_STATE_SLEEPY);
    if (sleepy_base) {
        mochi_state_config_t sleepy_cfg = *sleepy_base;
        sleepy_cfg.background.image.source = MOCHI_ASSET_EMBEDDED;
        sleepy_cfg.background.image.embedded = &icon_sample_16x16;
        mochi_configure_state(MOCHI_STATE_SLEEPY, &sleepy_cfg);
        ESP_LOGI(TAG, "SLEEPY: added embedded background image");
    }

    ESP_LOGI(TAG, "MochiState assets configured");
}
