/**
 * @file gallery_animations.h
 * @brief Car Animation Gallery - Non-face animation definitions
 *
 * Provides 36 creative animations beyond the mochi face system:
 * - Abstract Geometric (6): rings, spiral, heartbeat, orb, matrix, radar
 * - Weather Effects (6): rain, snow, sun, lightning, stars, aurora
 * - Emoji/Symbols (6): hearts, stars, ?, !, checkmark, X
 * - Tech/Digital (6): spinner, progress, waves, wifi, battery, binary
 * - Nature/Organic (6): ball, waves, butterfly, fireworks, fire, bubbles
 * - Dashboard/Automotive (6): speedometer, fuel, turn signals, hazard, RPM, gear
 */

#ifndef GALLERY_ANIMATIONS_H
#define GALLERY_ANIMATIONS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Animation Type Enumeration
 *===========================================================================*/

typedef enum {
    /* Abstract Geometric (33-38) */
    GALLERY_ANIM_PULSING_RINGS = 0,
    GALLERY_ANIM_SPIRAL_GALAXY,
    GALLERY_ANIM_HEARTBEAT,
    GALLERY_ANIM_BREATHING_ORB,
    GALLERY_ANIM_MATRIX_RAIN,
    GALLERY_ANIM_RADAR_SWEEP,

    /* Weather Effects (39-44) */
    GALLERY_ANIM_RAIN_STORM,
    GALLERY_ANIM_SNOWFALL,
    GALLERY_ANIM_SUNSHINE,
    GALLERY_ANIM_LIGHTNING,
    GALLERY_ANIM_STARRY_NIGHT,
    GALLERY_ANIM_AURORA,

    /* Emoji/Symbols (45-50) */
    GALLERY_ANIM_FLOATING_HEARTS,
    GALLERY_ANIM_STAR_BURST,
    GALLERY_ANIM_QUESTION_MARK,
    GALLERY_ANIM_EXCLAMATION,
    GALLERY_ANIM_CHECKMARK,
    GALLERY_ANIM_X_MARK,

    /* Tech/Digital (51-56) */
    GALLERY_ANIM_LOADING_SPINNER,
    GALLERY_ANIM_PROGRESS_BAR,
    GALLERY_ANIM_SOUND_WAVES,
    GALLERY_ANIM_WIFI_SIGNAL,
    GALLERY_ANIM_BATTERY_CHARGING,
    GALLERY_ANIM_BINARY_CODE,

    /* Nature/Organic (57-62) */
    GALLERY_ANIM_BOUNCING_BALL,
    GALLERY_ANIM_OCEAN_WAVES,
    GALLERY_ANIM_BUTTERFLY,
    GALLERY_ANIM_FIREWORKS,
    GALLERY_ANIM_CAMPFIRE,
    GALLERY_ANIM_BUBBLES,

    /* Dashboard/Automotive (63-68) */
    GALLERY_ANIM_SPEEDOMETER,
    GALLERY_ANIM_FUEL_GAUGE,
    GALLERY_ANIM_TURN_LEFT,
    GALLERY_ANIM_TURN_RIGHT,
    GALLERY_ANIM_HAZARD_LIGHTS,
    GALLERY_ANIM_GEAR_DISPLAY,

    GALLERY_ANIM_MAX
} gallery_anim_id_t;

/*===========================================================================
 * Animation Metadata
 *===========================================================================*/

typedef struct {
    const char *name;           /* Display name */
    const char *description;    /* Short description */
    uint32_t primary_color;     /* Primary theme color (hex) */
    uint32_t secondary_color;   /* Secondary color (hex) */
} gallery_anim_info_t;

/*===========================================================================
 * Public API
 *===========================================================================*/

/**
 * @brief Initialize the gallery animation system
 *
 * Creates the drawing object and animation timer.
 * Must be called before setting any animation.
 *
 * @param parent Parent LVGL object to create drawing area on
 * @return 0 on success, -1 on error
 */
int gallery_anim_init(lv_obj_t *parent);

/**
 * @brief Deinitialize the gallery animation system
 *
 * Stops timer and cleans up drawing objects.
 */
void gallery_anim_deinit(void);

/**
 * @brief Set the current animation
 *
 * @param anim_id Animation ID from gallery_anim_id_t
 */
void gallery_anim_set(gallery_anim_id_t anim_id);

/**
 * @brief Get the current animation ID
 *
 * @return Current animation ID
 */
gallery_anim_id_t gallery_anim_get(void);

/**
 * @brief Get animation info/metadata
 *
 * @param anim_id Animation ID
 * @return Pointer to animation info, or NULL if invalid
 */
const gallery_anim_info_t* gallery_anim_get_info(gallery_anim_id_t anim_id);

/**
 * @brief Get the total number of gallery animations
 *
 * @return Number of animations (36)
 */
int gallery_anim_get_count(void);

/**
 * @brief Show/hide the gallery animation drawing area
 *
 * @param visible true to show, false to hide
 */
void gallery_anim_set_visible(bool visible);

/**
 * @brief Check if gallery animation is visible
 *
 * @return true if visible
 */
bool gallery_anim_is_visible(void);

#ifdef __cplusplus
}
#endif

#endif /* GALLERY_ANIMATIONS_H */
