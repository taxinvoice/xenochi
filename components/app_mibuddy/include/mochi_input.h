/**
 * @file mochi_input.h
 * @brief MochiState Input System - Maps sensor/system inputs to MochiState
 *
 * This module provides an input-driven state machine that:
 * - Collects inputs from sensors (IMU, battery, RTC) and system state (wifi, touch)
 * - Computes derived/calculated variables from raw inputs
 * - Runs a user-defined mapper function to determine MochiState
 * - Optionally queries an external API for complex decisions
 *
 * Architecture:
 *   Sensors/System → mochi_input_state_t → Mapper Function → mochi_set()
 *
 * Usage:
 *   mochi_input_init();
 *   mochi_input_set_api_url("http://your-server:8080/mochi/state");
 *   mochi_input_set_mapper_fn(my_mapper);
 *   // In your update loop (e.g., 100ms timer):
 *   mochi_input_update();
 *
 * @see MOCHI_API.md for full API server specification
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "mochi_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Input State Structure
 *===========================================================================*/

/**
 * @brief All inputs available to the mapper function
 */
typedef struct {
    /* ── Static Variables (direct sensor/system readings) ── */

    /* Battery (AXP2101) */
    float battery_pct;           /**< Battery percentage 0-100 */
    bool  is_charging;           /**< USB power connected */
    float temperature;           /**< Battery/board temperature in Celsius */

    /* Time (RTC/SNTP) */
    int   hour;                  /**< Hour of day 0-23 */
    int   minute;                /**< Minute 0-59 */
    int   day_of_week;           /**< 0=Sunday, 6=Saturday */

    /* Motion (QMI8658C) */
    float accel_x;               /**< Acceleration X in g */
    float accel_y;               /**< Acceleration Y in g */
    float accel_z;               /**< Acceleration Z in g */
    float gyro_x;                /**< Gyroscope X in deg/s */
    float gyro_y;                /**< Gyroscope Y in deg/s */
    float gyro_z;                /**< Gyroscope Z in deg/s */

    /* System */
    bool  wifi_connected;        /**< WiFi station connected */
    bool  touch_active;          /**< Touch currently pressed */

    /* ── Calculated Variables (derived from static) ── */

    bool  is_low_battery;        /**< battery_pct < 20 */
    bool  is_critical_battery;   /**< battery_pct < 5 */
    float accel_magnitude;       /**< sqrt(x^2 + y^2 + z^2) */
    bool  is_moving;             /**< accel_magnitude > 0.3g (away from 1g rest) */
    bool  is_shaking;            /**< accel_magnitude > 2.0g (strong motion) */
    bool  is_night;              /**< hour in [22..6] */
    bool  is_weekend;            /**< Saturday or Sunday */

} mochi_input_state_t;

/*===========================================================================
 * Mapper Function Type
 *===========================================================================*/

/**
 * @brief User-defined mapper function signature
 *
 * Called by mochi_input_update() after collecting inputs.
 * The function should examine inputs and set output state/activity.
 *
 * @param input Current input state (read-only)
 * @param out_state Output: desired mochi state
 * @param out_activity Output: desired mochi activity
 */
typedef void (*mochi_mapper_fn_t)(
    const mochi_input_state_t *input,
    mochi_state_t *out_state,
    mochi_activity_t *out_activity
);

/*===========================================================================
 * Public API - Lifecycle
 *===========================================================================*/

/**
 * @brief Initialize the input system
 *
 * Call after mochi_init(). Sets up internal state and defaults.
 *
 * @return ESP_OK on success
 */
esp_err_t mochi_input_init(void);

/**
 * @brief Deinitialize the input system
 */
void mochi_input_deinit(void);

/*===========================================================================
 * Public API - Update Loop
 *===========================================================================*/

/**
 * @brief Collect inputs and run mapper
 *
 * This is the main update function. Call periodically (e.g., from timer).
 * 1. Collects static inputs from sensors/system
 * 2. Computes calculated variables
 * 3. Calls mapper function (if set)
 * 4. Calls mochi_set() with resulting state/activity
 *
 * @return ESP_OK on success
 */
esp_err_t mochi_input_update(void);

/**
 * @brief Get current input state (read-only)
 *
 * Returns pointer to the current input state. Valid until next update.
 *
 * @return Pointer to current input state
 */
const mochi_input_state_t* mochi_input_get(void);

/*===========================================================================
 * Public API - Mapper Configuration
 *===========================================================================*/

/**
 * @brief Set the mapper function
 *
 * The mapper function is called during mochi_input_update() to determine
 * which state/activity to set based on current inputs.
 *
 * @param fn Mapper function (NULL to disable auto-mapping)
 */
void mochi_input_set_mapper_fn(mochi_mapper_fn_t fn);

/*===========================================================================
 * Public API - API Helper (Async)
 *
 * The API query runs in a background FreeRTOS task to avoid blocking LVGL.
 * Use request/get pattern: request a query, then check for results later.
 *===========================================================================*/

/**
 * @brief Set API endpoint URL for remote decisions
 *
 * @param url HTTP endpoint URL (e.g., "http://10.0.13.101:8080/mochi/state")
 */
void mochi_input_set_api_url(const char *url);

/**
 * @brief Request an async API query (non-blocking)
 *
 * Schedules an HTTP request to be performed in a background task.
 * Check for results later with mochi_input_get_api_result().
 *
 * Request format:
 *   POST <url>
 *   { "battery": 85, "hour": 14, "moving": true, "shaking": false, ... }
 *
 * Response format:
 *   { "state": "EXCITED", "activity": "BOUNCE" }
 *
 * @param input Current input state to send
 */
void mochi_input_request_api_query(const mochi_input_state_t *input);

/**
 * @brief Check if an async API result is available
 *
 * Call this periodically (e.g., in your mapper) to check if the
 * background API query has completed.
 *
 * @param out_state Output: state from API response (if result ready)
 * @param out_activity Output: activity from API response (if result ready)
 * @return true if result was available and copied to outputs, false otherwise
 */
bool mochi_input_get_api_result(
    mochi_state_t *out_state,
    mochi_activity_t *out_activity
);

#ifdef __cplusplus
}
#endif
