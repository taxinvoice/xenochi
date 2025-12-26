/**
 * @file power_manager.h
 * @brief Power management for face-down sleep mode
 *
 * Implements automatic screen dimming and light sleep when the device
 * is placed face-down. Configurable timeouts with NVS persistence.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Power manager state
 */
typedef enum {
    POWER_STATE_ACTIVE,         /**< Normal operation, screen on */
    POWER_STATE_SCREEN_OFF,     /**< Screen faded to off, system running */
    POWER_STATE_LIGHT_SLEEP,    /**< ESP32-C6 in light sleep mode */
} power_state_t;

/**
 * @brief Power manager configuration
 */
typedef struct {
    uint32_t screen_off_timeout_sec;       /**< Seconds face-down before screen off */
    uint32_t light_sleep_timeout_sec;      /**< Seconds before light sleep (after screen off) */
    uint32_t idle_screen_off_timeout_sec;  /**< Seconds idle before screen off */
} power_manager_config_t;

/**
 * @brief Power state change callback type
 * @param old_state Previous power state
 * @param new_state New power state
 */
typedef void (*power_state_cb_t)(power_state_t old_state, power_state_t new_state);

/**
 * @brief Register a callback for power state changes
 *
 * The callback is invoked on every state transition (ACTIVE, SCREEN_OFF, LIGHT_SLEEP).
 * Only one callback can be registered at a time.
 *
 * @param cb Callback function, or NULL to unregister
 */
void power_manager_register_callback(power_state_cb_t cb);

/**
 * @brief Initialize power manager
 *
 * Starts the power manager task that monitors face-down state
 * and manages power transitions. Loads configuration from NVS.
 *
 * @return ESP_OK on success
 */
esp_err_t power_manager_init(void);

/**
 * @brief Deinitialize power manager
 *
 * Stops the power manager task and cleans up resources.
 */
void power_manager_deinit(void);

/**
 * @brief Get current power state
 * @return Current power state
 */
power_state_t power_manager_get_state(void);

/**
 * @brief Get current configuration
 * @param[out] config Pointer to store configuration
 * @return ESP_OK on success
 */
esp_err_t power_manager_get_config(power_manager_config_t *config);

/**
 * @brief Set screen-off timeout
 * @param seconds Timeout in seconds (10-600)
 * @return ESP_OK on success
 */
esp_err_t power_manager_set_screen_off_timeout(uint32_t seconds);

/**
 * @brief Set light-sleep timeout
 * @param seconds Timeout in seconds (60-1800)
 * @return ESP_OK on success
 */
esp_err_t power_manager_set_sleep_timeout(uint32_t seconds);

/**
 * @brief Get screen-off timeout
 * @return Timeout in seconds
 */
uint32_t power_manager_get_screen_off_timeout(void);

/**
 * @brief Get light-sleep timeout
 * @return Timeout in seconds
 */
uint32_t power_manager_get_sleep_timeout(void);

/**
 * @brief Set idle screen-off timeout
 * @param seconds Timeout in seconds (60-1800), 0 to disable
 * @return ESP_OK on success
 */
esp_err_t power_manager_set_idle_timeout(uint32_t seconds);

/**
 * @brief Get idle screen-off timeout
 * @return Timeout in seconds (0 = disabled)
 */
uint32_t power_manager_get_idle_timeout(void);

/**
 * @brief Inhibit or allow sleep
 *
 * Use this to temporarily prevent sleep (e.g., during audio playback).
 *
 * @param inhibit true to prevent sleep, false to allow
 */
void power_manager_inhibit_sleep(bool inhibit);

/**
 * @brief Check if sleep is currently inhibited
 * @return true if sleep is inhibited
 */
bool power_manager_is_sleep_inhibited(void);

/**
 * @brief Force wake from any sleep state
 *
 * Call this when user interaction is detected to immediately
 * restore full operation.
 */
void power_manager_wake(void);

#ifdef __cplusplus
}
#endif
