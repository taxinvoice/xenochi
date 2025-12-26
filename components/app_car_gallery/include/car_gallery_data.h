/**
 * @file car_gallery_data.h
 * @brief Car Animation Gallery - Animation catalog definitions
 *
 * Defines 32 car-themed animations with their mochi states, activities,
 * and trigger descriptions for future car integration.
 */
#pragma once

#include <stdint.h>
#include "mochi_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Animation Categories
 *===========================================================================*/

typedef enum {
    CAR_CAT_ENGINE = 0,      /**< Engine/Startup states */
    CAR_CAT_DRIVING,         /**< Driving dynamics */
    CAR_CAT_SPEED,           /**< Speed zones */
    CAR_CAT_ENVIRONMENT,     /**< Weather/Environment */
    CAR_CAT_PARKING,         /**< Parking/Maneuvers */
    CAR_CAT_SAFETY,          /**< Safety alerts */
    CAR_CAT_ENTERTAINMENT,   /**< Entertainment/Mood */
    CAR_CAT_ALL,             /**< Show all (no filter) */
    CAR_CAT_MAX
} car_category_t;

/*===========================================================================
 * Animation Entry Structure
 *===========================================================================*/

/**
 * @brief Single animation entry in the gallery
 */
typedef struct {
    const char *name;              /**< Display name (e.g., "Ignition") */
    mochi_state_t state;           /**< Mochi emotional state */
    mochi_activity_t activity;     /**< Mochi animation activity */
    mochi_theme_id_t theme;        /**< Suggested color theme */
    const char *trigger_desc;      /**< Human-readable trigger description */
    car_category_t category;       /**< Category for filtering */
} car_animation_t;

/*===========================================================================
 * Animation Catalog Access
 *===========================================================================*/

/**
 * @brief Get the full animation catalog
 *
 * @return Pointer to array of car_animation_t
 */
const car_animation_t* car_gallery_get_animations(void);

/**
 * @brief Get total number of animations
 *
 * @return Animation count (32)
 */
int car_gallery_get_count(void);

/**
 * @brief Get category name as string
 *
 * @param cat Category ID
 * @return Category name (e.g., "Engine")
 */
const char* car_gallery_category_name(car_category_t cat);

/*===========================================================================
 * Gallery UI Functions
 *===========================================================================*/

/**
 * @brief Initialize gallery UI
 *
 * Creates header, preview area, and info panel.
 *
 * @param parent Parent LVGL object (screen)
 * @return 0 on success
 */
int car_gallery_ui_init(lv_obj_t *parent);

/**
 * @brief Cleanup gallery UI
 */
void car_gallery_ui_deinit(void);

/**
 * @brief Navigate to next animation
 */
void car_gallery_next(void);

/**
 * @brief Navigate to previous animation
 */
void car_gallery_prev(void);

/**
 * @brief Set category filter
 *
 * @param cat Category to show (CAR_CAT_ALL for no filter)
 */
void car_gallery_set_category(car_category_t cat);

/**
 * @brief Get current animation index
 *
 * @return Current index in filtered list
 */
int car_gallery_get_current_index(void);

/**
 * @brief Get current animation
 *
 * @return Pointer to current animation entry
 */
const car_animation_t* car_gallery_get_current(void);

#ifdef __cplusplus
}
#endif
