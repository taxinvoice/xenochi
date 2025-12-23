/**
 * @file mochi_state.h
 * @brief MochiState System - Cute animated avatar with expressions and activities
 *
 * This module provides a state-based avatar system with:
 * - 8 emotional states (Happy, Excited, Worried, Cool, Dizzy, Panic, Sleepy, Shocked)
 * - 9 activity animations (Idle, Shake, Bounce, Spin, Wiggle, Nod, Blink, Snore, Vibrate)
 * - 5 color themes (Sakura, Mint, Lavender, Peach, Cloud)
 * - Particle effects (float, burst, sweat, sparkle, spiral, zzz)
 *
 * Usage:
 *   mochi_init();
 *   mochi_create(lv_screen_active());
 *   mochi_set(MOCHI_STATE_HAPPY, MOCHI_ACTIVITY_BOUNCE);
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * State Enumeration (Primary Emotion)
 *===========================================================================*/

typedef enum {
    MOCHI_STATE_HAPPY = 0,   /**< Default positive state */
    MOCHI_STATE_EXCITED,     /**< High energy positive */
    MOCHI_STATE_WORRIED,     /**< Concerned/anxious */
    MOCHI_STATE_COOL,        /**< Confident/chill */
    MOCHI_STATE_DIZZY,       /**< Confused/disoriented */
    MOCHI_STATE_PANIC,       /**< Alarmed/scared */
    MOCHI_STATE_SLEEPY,      /**< Tired/drowsy */
    MOCHI_STATE_SHOCKED,     /**< Surprised */
    MOCHI_STATE_MAX
} mochi_state_t;

/*===========================================================================
 * Activity Enumeration (Sub-state/Variation)
 *===========================================================================*/

typedef enum {
    MOCHI_ACTIVITY_IDLE = 0, /**< Default gentle breathing */
    MOCHI_ACTIVITY_SHAKE,    /**< Rapid left-right shake */
    MOCHI_ACTIVITY_BOUNCE,   /**< Up-down bouncing */
    MOCHI_ACTIVITY_SPIN,     /**< Slow rotation */
    MOCHI_ACTIVITY_WIGGLE,   /**< Side-to-side wobble */
    MOCHI_ACTIVITY_NOD,      /**< Up-down nod */
    MOCHI_ACTIVITY_BLINK,    /**< Periodic eye blinks */
    MOCHI_ACTIVITY_SNORE,    /**< Breathing + zzz (for sleepy) */
    MOCHI_ACTIVITY_VIBRATE,  /**< Fast micro-shake (for panic) */
    MOCHI_ACTIVITY_MAX
} mochi_activity_t;

/*===========================================================================
 * Theme Enumeration
 *===========================================================================*/

typedef enum {
    MOCHI_THEME_SAKURA = 0,  /**< Pink/Rose theme */
    MOCHI_THEME_MINT,        /**< Teal/Aqua theme */
    MOCHI_THEME_LAVENDER,    /**< Purple theme */
    MOCHI_THEME_PEACH,       /**< Orange/Coral theme */
    MOCHI_THEME_CLOUD,       /**< Blue/Sky theme */
    MOCHI_THEME_MAX
} mochi_theme_id_t;

/*===========================================================================
 * Mouth Type Enumeration
 *===========================================================================*/

typedef enum {
    MOCHI_MOUTH_SMILE = 0,   /**< Curved smile line */
    MOCHI_MOUTH_OPEN_SMILE,  /**< Open mouth with teeth/tongue */
    MOCHI_MOUTH_SMALL_O,     /**< Small O shape */
    MOCHI_MOUTH_SMIRK,       /**< Angled smirk */
    MOCHI_MOUTH_FLAT,        /**< Horizontal line */
    MOCHI_MOUTH_WAVY,        /**< Animated wavy line */
    MOCHI_MOUTH_SCREAM,      /**< Large O scream */
    MOCHI_MOUTH_MAX
} mochi_mouth_type_t;

/*===========================================================================
 * Particle Type Enumeration
 *===========================================================================*/

typedef enum {
    MOCHI_PARTICLE_NONE = 0, /**< No particles */
    MOCHI_PARTICLE_FLOAT,    /**< Gentle floating circles */
    MOCHI_PARTICLE_BURST,    /**< Expanding ring of circles */
    MOCHI_PARTICLE_SWEAT,    /**< Falling sweat drops */
    MOCHI_PARTICLE_SPARKLE,  /**< Rotating star shapes */
    MOCHI_PARTICLE_SPIRAL,   /**< Rotating spiral symbols */
    MOCHI_PARTICLE_ZZZ,      /**< Floating Z letters */
    MOCHI_PARTICLE_MAX
} mochi_particle_type_t;

/*===========================================================================
 * Face Parameters Structure
 *===========================================================================*/

/**
 * @brief Parameters controlling face appearance and animation
 */
typedef struct {
    /* Eye parameters */
    float eye_scale;         /**< Eye size multiplier (0.1 to 1.4) */
    float eye_offset_x;      /**< Horizontal eye offset in pixels */
    float eye_offset_y;      /**< Vertical eye offset in pixels */
    float pupil_size;        /**< Pupil size multiplier (0.3 to 1.3) */
    float eye_squish;        /**< Vertical squish (0 = normal, 0.8 = nearly closed) */

    /* Mouth parameters */
    mochi_mouth_type_t mouth_type;  /**< Mouth shape type */
    float mouth_open;        /**< Mouth openness (0.2 to 1.0) */

    /* Face animation */
    float face_squish;       /**< Breathing effect (-0.05 to 0.05) */
    float face_offset_y;     /**< Vertical bounce offset */
    float face_rotation;     /**< Rotation angle in degrees */

    /* Effects */
    bool show_blush;         /**< Show blush circles */
    bool show_sparkles;      /**< Show eye sparkles */
    mochi_particle_type_t particle_type;  /**< Particle effect type */
} mochi_face_params_t;

/*===========================================================================
 * Public API - Lifecycle
 *===========================================================================*/

/**
 * @brief Asset setup callback type
 *
 * Called after mochi_init() to configure embedded assets for states.
 */
typedef void (*mochi_asset_setup_fn)(void);

/**
 * @brief Register asset setup callback
 *
 * Call this BEFORE mochi_init() to register a function that will configure
 * embedded assets for MochiStates. The callback is invoked during mochi_init().
 *
 * @param setup_fn Function to call for asset setup (NULL to clear)
 */
void mochi_register_asset_setup(mochi_asset_setup_fn setup_fn);

/**
 * @brief Initialize the mochi state system
 *
 * Must be called before any other mochi_* functions.
 * Initializes internal state and default configurations.
 * If an asset setup callback was registered, it will be called.
 *
 * @return ESP_OK on success
 */
esp_err_t mochi_init(void);

/**
 * @brief Deinitialize and cleanup mochi state system
 *
 * Stops all animations and frees resources.
 */
void mochi_deinit(void);

/**
 * @brief Create mochi avatar UI on parent object
 *
 * Creates the LVGL canvas and all visual elements.
 * Starts with MOCHI_STATE_HAPPY, MOCHI_ACTIVITY_IDLE.
 *
 * @param parent Parent LVGL object
 * @return ESP_OK on success
 */
esp_err_t mochi_create(lv_obj_t *parent);

/*===========================================================================
 * Public API - State Control
 *===========================================================================*/

/**
 * @brief Set current state and activity
 *
 * This is the primary API. Sets both state (emotion) and activity (animation).
 * Automatically updates visuals, particles, and optional audio.
 *
 * @param state Emotional state
 * @param activity Animation activity
 * @return ESP_OK on success
 */
esp_err_t mochi_set(mochi_state_t state, mochi_activity_t activity);

/**
 * @brief Set state only (keeps current activity or uses default)
 *
 * @param state Emotional state
 * @return ESP_OK on success
 */
esp_err_t mochi_set_state(mochi_state_t state);

/**
 * @brief Set activity only (keeps current state)
 *
 * @param activity Animation activity
 * @return ESP_OK on success
 */
esp_err_t mochi_set_activity(mochi_activity_t activity);

/**
 * @brief Get current state
 *
 * @return Current emotional state
 */
mochi_state_t mochi_get_state(void);

/**
 * @brief Get current activity
 *
 * @return Current animation activity
 */
mochi_activity_t mochi_get_activity(void);

/**
 * @brief Get state name as string
 *
 * @param state State to get name for
 * @return State name string (e.g., "Happy")
 */
const char* mochi_state_name(mochi_state_t state);

/**
 * @brief Get activity name as string
 *
 * @param activity Activity to get name for
 * @return Activity name string (e.g., "Bounce")
 */
const char* mochi_activity_name(mochi_activity_t activity);

/*===========================================================================
 * Public API - Theme Control
 *===========================================================================*/

/**
 * @brief Set color theme
 *
 * Theme is a global setting that persists across state changes.
 * All rendering uses the current theme colors.
 *
 * @param theme Theme ID
 * @return ESP_OK on success
 */
esp_err_t mochi_set_theme(mochi_theme_id_t theme);

/**
 * @brief Get current theme
 *
 * @return Current theme ID
 */
mochi_theme_id_t mochi_get_theme(void);

/**
 * @brief Cycle to next theme
 *
 * Wraps around from last theme to first.
 */
void mochi_next_theme(void);

/**
 * @brief Get theme name as string
 *
 * @param theme Theme ID
 * @return Theme name string (e.g., "Sakura")
 */
const char* mochi_theme_name(mochi_theme_id_t theme);

/*===========================================================================
 * Public API - Animation Control
 *===========================================================================*/

/**
 * @brief Set animation intensity
 *
 * Controls how pronounced animations are (0.2 to 1.0).
 *
 * @param intensity Animation intensity multiplier
 */
void mochi_set_intensity(float intensity);

/**
 * @brief Get current animation intensity
 *
 * @return Current intensity value
 */
float mochi_get_intensity(void);

/*===========================================================================
 * Public API - Lifecycle Hooks
 *===========================================================================*/

/**
 * @brief Pause all animations
 *
 * Call when app goes to background.
 */
void mochi_pause(void);

/**
 * @brief Resume all animations
 *
 * Call when app returns to foreground.
 */
void mochi_resume(void);

/*===========================================================================
 * Public API - Audio (Optional)
 *===========================================================================*/

/**
 * @brief Play sound for current state
 *
 * @param path Audio file path (e.g., "/sdcard/sounds/happy.mp3")
 * @param loop True for looping, false for one-shot
 */
void mochi_play_sound(const char *path, bool loop);

/**
 * @brief Stop current sound
 */
void mochi_stop_sound(void);

/*===========================================================================
 * Public API - Asset Configuration
 *===========================================================================*/

/* Forward declaration - full definition in mochi_assets.h */
struct mochi_state_config_t;
typedef struct mochi_state_config_t mochi_state_config_t;

/**
 * @brief Configure assets for a state
 *
 * Associates sounds, sprites, and backgrounds with a state.
 * Assets can be embedded (flash) or on SD card.
 *
 * @param state State to configure
 * @param config Asset configuration (NULL to clear)
 * @return ESP_OK on success
 *
 * @example
 *   #include "mochi_assets.h"
 *   mochi_state_config_t cfg = {
 *       .enter_sound = MOCHI_SOUND_EMBEDDED(beep_pcm, beep_len, 8000),
 *       .sprite = MOCHI_IMAGE_SD("happy.png"),
 *   };
 *   mochi_configure_state(MOCHI_STATE_HAPPY, &cfg);
 */
esp_err_t mochi_configure_state(mochi_state_t state, const mochi_state_config_t *config);

/**
 * @brief Get asset configuration for a state
 *
 * @param state State to query
 * @return Pointer to config (NULL if invalid state)
 */
const mochi_state_config_t* mochi_get_state_config(mochi_state_t state);

#ifdef __cplusplus
}
#endif
