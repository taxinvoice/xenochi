/**
 * @file sd_logger.h
 * @brief SD Card Logger API
 *
 * Logs ESP-IDF log output (ESP_LOGI, ESP_LOGW, etc.) to SD card files.
 *
 * Features:
 * - Hooks into ESP-IDF logging via esp_log_set_vprintf()
 * - Timestamps each log line: [YYYY-MM-DD HH:MM:SS]
 * - Automatic file rotation when size limit reached
 * - NVS persistence for enabled state
 * - Console passthrough (logs still appear on serial)
 *
 * Usage:
 *   1. Configure via `idf.py menuconfig` -> SD Card Logger Configuration
 *   2. Call sd_logger_init() after sd_card_init() in app_main()
 *   3. Use sd_logger_set_enabled(true) to start logging
 *
 * @note Requires SD card to be mounted before initialization
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Logger configuration structure
 */
typedef struct {
    bool enabled;               /**< Logging enabled flag */
    uint32_t max_file_size_kb;  /**< Max size per log file in KB */
    uint8_t max_files;          /**< Max number of rotated files to keep */
    char log_dir[64];           /**< Log directory path */
} sd_logger_config_t;

/**
 * @brief Initialize the SD logger
 *
 * Loads configuration from NVS and hooks into ESP-IDF logging.
 * Must be called after sd_card_init().
 *
 * @return ESP_OK on success
 * @return ESP_FAIL if initialization failed
 */
esp_err_t sd_logger_init(void);

/**
 * @brief Enable or disable file logging
 *
 * State is persisted to NVS.
 *
 * @param enabled true to enable, false to disable
 * @return ESP_OK on success
 * @return ESP_FAIL if state couldn't be saved
 */
esp_err_t sd_logger_set_enabled(bool enabled);

/**
 * @brief Check if logging is enabled
 *
 * @return true if logging is enabled
 * @return false if logging is disabled
 */
bool sd_logger_is_enabled(void);

/**
 * @brief Get the current log file path
 *
 * @return Pointer to current log file path string
 * @return NULL if logger not initialized
 */
const char* sd_logger_get_current_file(void);

/**
 * @brief Get total size of all log files in KB
 *
 * @return Total log size in KB
 */
uint32_t sd_logger_get_total_size_kb(void);

/**
 * @brief Get number of log files
 *
 * @return Number of log files in the log directory
 */
uint8_t sd_logger_get_file_count(void);

/**
 * @brief Clear all log files
 *
 * Deletes all .log files in the log directory.
 *
 * @return ESP_OK on success
 * @return ESP_FAIL if some files couldn't be deleted
 */
esp_err_t sd_logger_clear_all(void);

/**
 * @brief Flush pending writes to SD card
 *
 * Forces any buffered log data to be written immediately.
 */
void sd_logger_flush(void);

/**
 * @brief Deinitialize the SD logger
 *
 * Unhooks from ESP-IDF logging and closes log file.
 */
void sd_logger_deinit(void);

/**
 * @brief Get current logger configuration
 *
 * @param config Pointer to config structure to fill
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if config is NULL
 */
esp_err_t sd_logger_get_config(sd_logger_config_t *config);

#ifdef __cplusplus
}
#endif
