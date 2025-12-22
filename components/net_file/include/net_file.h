/**
 * @file net_file.h
 * @brief HTTP File Transfer for ESP32 - Download/upload files over HTTP/HTTPS
 *
 * This module provides:
 * - File download to SD card or memory buffer
 * - File upload via HTTP POST (multipart/form-data)
 * - Progress callbacks for UI updates
 * - Automatic resume for interrupted downloads (if server supports Range)
 *
 * @note This is a STUB implementation - functions return ESP_ERR_NOT_SUPPORTED
 * @todo Implement using esp_http_client with chunked transfer
 */

#pragma once

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Progress callback for file transfers
 *
 * @param bytes_transferred Bytes transferred so far
 * @param total_size Total file size (0 if unknown)
 */
typedef void (*net_file_progress_cb_t)(size_t bytes_transferred, size_t total_size);

/**
 * @brief Download a file from URL to SD card
 *
 * @param url URL to download from
 * @param dest_path Destination path on SD card (e.g., "/sdcard/file.bin")
 * @param progress_cb Progress callback (can be NULL)
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED (stub)
 */
esp_err_t net_file_download(const char *url, const char *dest_path, net_file_progress_cb_t progress_cb);

/**
 * @brief Download a file from URL into memory buffer
 *
 * @param url URL to download from
 * @param buffer Buffer to store file content
 * @param max_size Maximum buffer size
 * @param actual_size Pointer to store actual downloaded size
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED (stub)
 */
esp_err_t net_file_download_to_buffer(const char *url, void *buffer, size_t max_size, size_t *actual_size);

/**
 * @brief Upload a file from SD card via HTTP POST
 *
 * @param url URL to upload to
 * @param src_path Source file path on SD card
 * @param field_name Form field name for the file
 * @param progress_cb Progress callback (can be NULL)
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED (stub)
 */
esp_err_t net_file_upload(const char *url, const char *src_path, const char *field_name, net_file_progress_cb_t progress_cb);

#ifdef __cplusplus
}
#endif
