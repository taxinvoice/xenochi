/**
 * @file lvgl_mibuddy.h
 * @brief MiBuddy slideshow UI API
 *
 * Provides functions to create and control an image slideshow
 * that displays PNG images from the SD card.
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the MiBuddy slideshow UI
 *
 * Scans /sdcard/Images/ for PNG files and creates a fullscreen
 * image viewer that cycles through them every 2 seconds.
 *
 * @param parent The parent LVGL object to create the UI on
 */
void lvgl_mibuddy_create(lv_obj_t *parent);

/**
 * @brief Cleanup MiBuddy resources
 *
 * Stops the slideshow timer and frees allocated memory.
 * Call this when closing the app.
 */
void lvgl_mibuddy_cleanup(void);

/**
 * @brief Pause the slideshow
 *
 * Pauses the slideshow timer. Call when app goes to background.
 */
void lvgl_mibuddy_pause(void);

/**
 * @brief Resume the slideshow
 *
 * Resumes the slideshow timer. Call when app returns to foreground.
 */
void lvgl_mibuddy_resume(void);

#ifdef __cplusplus
}
#endif
