/**
 * @file motion_config.h
 * @brief Motion detection threshold configuration
 *
 * Provides configurable thresholds for motion detection with NVS persistence.
 * All thresholds stored as actual physical units (g-force, deg/s).
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Default Values
 *===========================================================================*/

#define MOTION_DEFAULT_MOVING_G       0.3f    /**< Deviation from 1g for is_moving */
#define MOTION_DEFAULT_SHAKING_G      2.0f    /**< Magnitude for is_shaking */
#define MOTION_DEFAULT_ROTATING_DPS   30.0f   /**< Gyro magnitude for is_rotating (deg/s) */
#define MOTION_DEFAULT_SPINNING_DPS   100.0f  /**< Gyro magnitude for is_spinning (deg/s) */
#define MOTION_DEFAULT_BRAKING_GPS    3.0f    /**< Deceleration rate for is_braking (g/s) */

/*===========================================================================
 * Configuration Structure
 *===========================================================================*/

/**
 * @brief Motion detection threshold configuration
 */
typedef struct {
    float moving_threshold_g;      /**< Deviation from 1g to trigger is_moving (default: 0.3g) */
    float shaking_threshold_g;     /**< Total magnitude to trigger is_shaking (default: 2.0g) */
    float rotating_threshold_dps;  /**< Gyro magnitude to trigger is_rotating (default: 30 deg/s) */
    float spinning_threshold_dps;  /**< Gyro magnitude to trigger is_spinning (default: 100 deg/s) */
    float braking_threshold_gps;   /**< Deceleration rate to trigger is_braking (default: 3.0 g/s) */
} motion_config_t;

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

/**
 * @brief Initialize motion config (load from NVS)
 * @return ESP_OK on success
 */
esp_err_t motion_config_init(void);

/**
 * @brief Deinitialize motion config
 */
void motion_config_deinit(void);

/*===========================================================================
 * Configuration Access
 *===========================================================================*/

/**
 * @brief Get direct pointer to config (for fast read in mochi_input)
 * @return Pointer to internal config (read-only access recommended)
 */
const motion_config_t* motion_config_get_ptr(void);

/**
 * @brief Get copy of current motion configuration
 * @param[out] config Pointer to store configuration
 * @return ESP_OK on success
 */
esp_err_t motion_config_get(motion_config_t *config);

/*===========================================================================
 * Individual Setters (save to NVS immediately)
 *===========================================================================*/

esp_err_t motion_config_set_moving_threshold(float g);
esp_err_t motion_config_set_shaking_threshold(float g);
esp_err_t motion_config_set_rotating_threshold(float dps);
esp_err_t motion_config_set_spinning_threshold(float dps);
esp_err_t motion_config_set_braking_threshold(float gps);

/*===========================================================================
 * Individual Getters
 *===========================================================================*/

float motion_config_get_moving_threshold(void);
float motion_config_get_shaking_threshold(void);
float motion_config_get_rotating_threshold(void);
float motion_config_get_spinning_threshold(void);
float motion_config_get_braking_threshold(void);

/*===========================================================================
 * Utilities
 *===========================================================================*/

/**
 * @brief Reset all thresholds to defaults and save to NVS
 * @return ESP_OK on success
 */
esp_err_t motion_config_reset_defaults(void);

#ifdef __cplusplus
}
#endif
