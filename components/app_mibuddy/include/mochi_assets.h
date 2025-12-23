/**
 * @file mochi_assets.h
 * @brief Asset management for MochiState - supports embedded and SD card assets
 *
 * This module provides a unified asset system that can load sounds and images
 * from either embedded flash memory or SD card storage.
 *
 * Usage:
 *   // Configure a state with embedded sound
 *   mochi_state_config_t cfg = {
 *       .enter_sound = MOCHI_SOUND_EMBEDDED(beep_pcm, beep_len, 8000),
 *       .sprite = MOCHI_IMAGE_EMBEDDED(icon_happy),
 *   };
 *   mochi_configure_state(MOCHI_STATE_HAPPY, &cfg);
 *
 *   // Or with SD card assets
 *   mochi_state_config_t cfg = {
 *       .enter_sound = MOCHI_SOUND_SD("happy.mp3"),
 *       .background = MOCHI_IMAGE_SD("bg_happy.png"),
 *   };
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"
#include "mochi_state.h"  /* For mochi_face_params_t, enums */

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * SD Card Asset Paths
 *===========================================================================*/

#define MOCHI_SD_SOUNDS_PATH    "/sdcard/Sounds/"
#define MOCHI_SD_IMAGES_PATH    "/sdcard/Images/"

/*===========================================================================
 * Asset Source Enumeration
 *===========================================================================*/

/**
 * @brief Asset source type
 */
typedef enum {
    MOCHI_ASSET_NONE = 0,    /**< No asset configured */
    MOCHI_ASSET_EMBEDDED,    /**< Asset embedded in flash */
    MOCHI_ASSET_SDCARD       /**< Asset on SD card */
} mochi_asset_source_t;

/*===========================================================================
 * Sound Asset Structure
 *===========================================================================*/

/**
 * @brief Embedded PCM audio data descriptor
 */
typedef struct {
    const int16_t *pcm_data;  /**< Pointer to PCM sample array */
    size_t pcm_len;           /**< Number of samples */
    uint32_t sample_rate;     /**< Sample rate in Hz (e.g., 8000, 44100) */
    uint8_t channels;         /**< Number of channels (1=mono, 2=stereo) */
} mochi_embedded_sound_t;

/**
 * @brief Sound asset - can be embedded PCM or SD card file
 */
typedef struct {
    mochi_asset_source_t source;    /**< Asset source type */
    union {
        mochi_embedded_sound_t embedded;  /**< Embedded PCM data */
        const char *sd_path;              /**< SD card filename (e.g., "beep.mp3") */
    };
} mochi_sound_asset_t;

/*===========================================================================
 * Image Asset Structure
 *===========================================================================*/

/**
 * @brief Image asset - can be embedded LVGL image or SD card file
 */
typedef struct {
    mochi_asset_source_t source;    /**< Asset source type */
    union {
        const lv_image_dsc_t *embedded;  /**< Embedded LVGL 9.x image descriptor */
        const char *sd_path;              /**< SD card filename (e.g., "icon.png") */
    };
} mochi_image_asset_t;

/*===========================================================================
 * State Configuration Structure
 *===========================================================================*/

/**
 * @brief Complete configuration for a MochiState
 *
 * Organized into three major sections:
 * - Background: Background image layer
 * - Foreground: Face appearance + sprite overlay
 * - Audio: Enter sound + looping sound
 */
typedef struct mochi_state_config_t {

    /*=========================================================================
     * BACKGROUND LAYER
     *=========================================================================*/
    struct {
        mochi_image_asset_t image;     /**< Background image (behind face) */
    } background;

    /*=========================================================================
     * FOREGROUND LAYER (Face + Sprite Overlay)
     *=========================================================================*/
    struct {
        /* Face appearance */
        mochi_face_params_t face;      /**< Face parameters (eyes, mouth, particles) */

        /* Sprite overlay (on top of face) */
        mochi_image_asset_t sprite;    /**< Overlay sprite image */
        int16_t sprite_x;              /**< Sprite X offset from center */
        int16_t sprite_y;              /**< Sprite Y offset from center */
        uint8_t sprite_frames;         /**< Animation frames (1 = static) */
        uint16_t sprite_frame_ms;      /**< Frame duration in ms */
    } foreground;

    /*=========================================================================
     * AUDIO
     *=========================================================================*/
    struct {
        mochi_sound_asset_t enter;     /**< Played once when entering state */
        mochi_sound_asset_t loop;      /**< Looped continuously while in state */
    } audio;

} mochi_state_config_t;

/*===========================================================================
 * Helper Macros for Asset Definition
 *===========================================================================*/

/**
 * @brief Create an empty/disabled sound asset
 */
#define MOCHI_SOUND_NONE() \
    { .source = MOCHI_ASSET_NONE }

/**
 * @brief Create an embedded PCM sound asset
 * @param pcm Pointer to PCM data array
 * @param len Number of samples
 * @param rate Sample rate in Hz
 */
#define MOCHI_SOUND_EMBEDDED(pcm, len, rate) \
    { .source = MOCHI_ASSET_EMBEDDED, \
      .embedded = { .pcm_data = (pcm), .pcm_len = (len), .sample_rate = (rate), .channels = 1 } }

/**
 * @brief Create an embedded stereo PCM sound asset
 */
#define MOCHI_SOUND_EMBEDDED_STEREO(pcm, len, rate) \
    { .source = MOCHI_ASSET_EMBEDDED, \
      .embedded = { .pcm_data = (pcm), .pcm_len = (len), .sample_rate = (rate), .channels = 2 } }

/**
 * @brief Create an SD card sound asset
 * @param path Filename relative to /sdcard/Sounds/ (e.g., "beep.mp3")
 */
#define MOCHI_SOUND_SD(path) \
    { .source = MOCHI_ASSET_SDCARD, .sd_path = (path) }

/**
 * @brief Create an empty/disabled image asset
 */
#define MOCHI_IMAGE_NONE() \
    { .source = MOCHI_ASSET_NONE }

/**
 * @brief Create an embedded LVGL image asset
 * @param img_dsc LVGL image descriptor variable name (not pointer)
 */
#define MOCHI_IMAGE_EMBEDDED(img_dsc) \
    { .source = MOCHI_ASSET_EMBEDDED, .embedded = &(img_dsc) }

/**
 * @brief Create an SD card image asset
 * @param path Filename relative to /sdcard/Images/ (e.g., "icon.png")
 */
#define MOCHI_IMAGE_SD(path) \
    { .source = MOCHI_ASSET_SDCARD, .sd_path = (path) }

/**
 * @brief Initialize a state config with defaults (no assets configured)
 */
#define MOCHI_STATE_CONFIG_DEFAULT() \
    { \
        .background = { .image = MOCHI_IMAGE_NONE() }, \
        .foreground = { \
            .face = {0}, \
            .sprite = MOCHI_IMAGE_NONE(), \
            .sprite_x = 0, .sprite_y = 0, \
            .sprite_frames = 1, .sprite_frame_ms = 100 \
        }, \
        .audio = { \
            .enter = MOCHI_SOUND_NONE(), \
            .loop = MOCHI_SOUND_NONE() \
        } \
    }

/*===========================================================================
 * Face Parameter Helper Macros
 *===========================================================================*/

/**
 * @brief Define face parameters for a happy expression
 */
#define MOCHI_FACE_HAPPY() \
    { \
        .eye_scale = 1.0f, .eye_offset_x = 0.0f, .eye_offset_y = 0.0f, \
        .pupil_size = 1.0f, .eye_squish = 0.0f, \
        .mouth_type = MOCHI_MOUTH_SMILE, .mouth_open = 0.3f, \
        .face_squish = 0.0f, .face_offset_y = 0.0f, .face_rotation = 0.0f, \
        .show_blush = true, .show_sparkles = true, \
        .particle_type = MOCHI_PARTICLE_FLOAT \
    }

/**
 * @brief Define face parameters for an excited expression
 */
#define MOCHI_FACE_EXCITED() \
    { \
        .eye_scale = 0.8f, .eye_offset_x = 0.0f, .eye_offset_y = 3.0f, \
        .pupil_size = 0.7f, .eye_squish = 0.3f, \
        .mouth_type = MOCHI_MOUTH_OPEN_SMILE, .mouth_open = 0.7f, \
        .face_squish = 0.05f, .face_offset_y = 5.0f, .face_rotation = 0.0f, \
        .show_blush = true, .show_sparkles = true, \
        .particle_type = MOCHI_PARTICLE_BURST \
    }

/**
 * @brief Define face parameters for a sleepy expression
 */
#define MOCHI_FACE_SLEEPY() \
    { \
        .eye_scale = 0.15f, .eye_offset_x = 0.0f, .eye_offset_y = 5.0f, \
        .pupil_size = 0.5f, .eye_squish = 0.8f, \
        .mouth_type = MOCHI_MOUTH_SMILE, .mouth_open = 0.2f, \
        .face_squish = 0.0f, .face_offset_y = 3.0f, .face_rotation = -3.0f, \
        .show_blush = false, .show_sparkles = false, \
        .particle_type = MOCHI_PARTICLE_ZZZ \
    }

/**
 * @brief Define face parameters for a panicked expression
 */
#define MOCHI_FACE_PANIC() \
    { \
        .eye_scale = 1.4f, .eye_offset_x = 0.0f, .eye_offset_y = -3.0f, \
        .pupil_size = 0.4f, .eye_squish = -0.2f, \
        .mouth_type = MOCHI_MOUTH_SCREAM, .mouth_open = 1.0f, \
        .face_squish = 0.0f, .face_offset_y = 0.0f, .face_rotation = 0.0f, \
        .show_blush = false, .show_sparkles = false, \
        .particle_type = MOCHI_PARTICLE_SWEAT \
    }

/**
 * @brief Define custom face parameters
 */
#define MOCHI_FACE_CUSTOM(eye_sc, pupil_sz, mouth_ty, mouth_op, particle_ty) \
    { \
        .eye_scale = (eye_sc), .eye_offset_x = 0.0f, .eye_offset_y = 0.0f, \
        .pupil_size = (pupil_sz), .eye_squish = 0.0f, \
        .mouth_type = (mouth_ty), .mouth_open = (mouth_op), \
        .face_squish = 0.0f, .face_offset_y = 0.0f, .face_rotation = 0.0f, \
        .show_blush = false, .show_sparkles = false, \
        .particle_type = (particle_ty) \
    }

/*===========================================================================
 * Asset Playback API
 *===========================================================================*/

/**
 * @brief Play a sound asset
 *
 * Handles both embedded PCM and SD card files transparently.
 * For embedded PCM, resamples to codec sample rate if needed.
 *
 * @param asset Sound asset to play
 * @param loop True to loop continuously, false for one-shot
 * @return ESP_OK on success
 */
esp_err_t mochi_play_asset_sound(const mochi_sound_asset_t *asset, bool loop);

/**
 * @brief Stop current asset sound
 */
void mochi_stop_asset_sound(void);

/**
 * @brief Create an LVGL image object from image asset
 *
 * @param parent Parent LVGL object
 * @param asset Image asset to display
 * @return Created lv_obj_t* or NULL if asset is NONE
 */
lv_obj_t* mochi_create_asset_image(lv_obj_t *parent, const mochi_image_asset_t *asset);

/**
 * @brief Update existing image object with new asset
 *
 * @param img Existing lv_img object
 * @param asset New image asset
 * @return ESP_OK on success
 */
esp_err_t mochi_update_asset_image(lv_obj_t *img, const mochi_image_asset_t *asset);

#ifdef __cplusplus
}
#endif
