/**
 * @file sd_file.h
 * @brief SD Card File Operations API
 *
 * Provides basic file operations for SD card:
 * - Write, append, read, delete files
 * - Check file existence and size
 * - Create directories
 *
 * Prerequisites:
 * - SD card must be initialized via sd_card_init() before use
 * - Files are accessed relative to MOUNT_POINT (/sdcard)
 *
 * Thread Safety:
 * - All functions are thread-safe (use internal mutex)
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Write data to a file (overwrites existing content)
 *
 * Creates the file if it doesn't exist, or truncates existing file.
 *
 * @param path Full path to file (e.g., "/sdcard/data.txt")
 * @param data Pointer to data buffer
 * @param len Number of bytes to write
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if path or data is NULL
 * @return ESP_FAIL on write error
 */
esp_err_t sd_file_write(const char *path, const void *data, size_t len);

/**
 * @brief Write a string to a file (overwrites existing content)
 *
 * Convenience wrapper for sd_file_write() for null-terminated strings.
 *
 * @param path Full path to file
 * @param str Null-terminated string to write
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if path or str is NULL
 * @return ESP_FAIL on write error
 */
esp_err_t sd_file_write_string(const char *path, const char *str);

/**
 * @brief Append data to a file
 *
 * Creates the file if it doesn't exist.
 *
 * @param path Full path to file
 * @param data Pointer to data buffer
 * @param len Number of bytes to append
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if path or data is NULL
 * @return ESP_FAIL on write error
 */
esp_err_t sd_file_append(const char *path, const void *data, size_t len);

/**
 * @brief Append a string to a file
 *
 * Convenience wrapper for sd_file_append() for null-terminated strings.
 *
 * @param path Full path to file
 * @param str Null-terminated string to append
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if path or str is NULL
 * @return ESP_FAIL on write error
 */
esp_err_t sd_file_append_string(const char *path, const char *str);

/**
 * @brief Read file contents into buffer
 *
 * @param path Full path to file
 * @param buf Buffer to read data into
 * @param buf_len Size of buffer
 * @return Number of bytes read on success (may be less than buf_len)
 * @return -1 on error (file not found, read error, invalid args)
 */
int sd_file_read(const char *path, void *buf, size_t buf_len);

/**
 * @brief Delete a file
 *
 * @param path Full path to file
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if path is NULL
 * @return ESP_FAIL if file doesn't exist or delete failed
 */
esp_err_t sd_file_delete(const char *path);

/**
 * @brief Check if a file exists
 *
 * @param path Full path to file
 * @return true if file exists
 * @return false if file doesn't exist or path is NULL
 */
bool sd_file_exists(const char *path);

/**
 * @brief Get file size in bytes
 *
 * @param path Full path to file
 * @return File size in bytes on success
 * @return -1 on error (file not found, invalid path)
 */
int32_t sd_file_size(const char *path);

/**
 * @brief Create a directory (creates parent directories if needed)
 *
 * @param path Full path to directory
 * @return ESP_OK on success or if directory already exists
 * @return ESP_ERR_INVALID_ARG if path is NULL
 * @return ESP_FAIL on creation error
 */
esp_err_t sd_file_mkdir(const char *path);

/**
 * @brief Rename/move a file
 *
 * @param old_path Current file path
 * @param new_path New file path
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if paths are NULL
 * @return ESP_FAIL on rename error
 */
esp_err_t sd_file_rename(const char *old_path, const char *new_path);

#ifdef __cplusplus
}
#endif
