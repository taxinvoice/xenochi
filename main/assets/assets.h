/**
 * @file assets.h
 * @brief Shared assets header for Xenochi project
 *
 * This header declares both embedded assets (compiled into firmware)
 * and SD card paths for external assets.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

//==============================================================================
// Embedded Sound Assets (PCM data in flash)
//==============================================================================

/**
 * @brief 440Hz beep tone - 8kHz mono 16-bit PCM
 * Duration: ~100ms (800 samples)
 *
 * Usage: Feed to I2S after resampling to codec's sample rate,
 * or reconfigure codec to 8kHz for direct playback.
 */
extern const int16_t beep_8k_mono[];
extern const size_t beep_8k_mono_len;
extern const uint32_t beep_8k_mono_sample_rate;  // 8000
extern const uint8_t beep_8k_mono_channels;       // 1
extern const uint8_t beep_8k_mono_bits;           // 16

//==============================================================================
// Embedded Image Assets (LVGL format)
//==============================================================================

/**
 * @brief Sample 16x16 icon (cyan circle)
 * Format: RGB565, LVGL 9.x
 */
LV_IMAGE_DECLARE(icon_sample_16x16);

//==============================================================================
// SD Card Asset Paths
//==============================================================================

// Base paths for SD card assets
#define ASSET_SD_BASE       "/sdcard/assets"
#define ASSET_PATH_MUSIC    "/sdcard/assets/music/"
#define ASSET_PATH_SOUNDS   "/sdcard/assets/sounds/"
#define ASSET_PATH_IMAGES   "/sdcard/assets/images/"

// Legacy paths (existing music location)
#define ASSET_PATH_MUSIC_LEGACY "/sdcard/Music/"

//==============================================================================
// Asset Configuration
//==============================================================================

/**
 * @brief Configure MochiStates with embedded assets
 *
 * Call after mochi_init(), before mochi_create().
 * Adds embedded sounds/images to specific states.
 */
void mochi_setup_assets(void);

#ifdef __cplusplus
}
#endif
