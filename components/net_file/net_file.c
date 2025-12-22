/**
 * @file net_file.c
 * @brief HTTP File Transfer STUB implementation
 *
 * @todo Implement using esp_http_client with chunked transfer
 */

#include "net_file.h"
#include "esp_log.h"

static const char *TAG = "net_file";

esp_err_t net_file_download(const char *url, const char *dest_path, net_file_progress_cb_t progress_cb)
{
    ESP_LOGW(TAG, "STUB: net_file_download() not implemented");
    (void)url;
    (void)dest_path;
    (void)progress_cb;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t net_file_download_to_buffer(const char *url, void *buffer, size_t max_size, size_t *actual_size)
{
    ESP_LOGW(TAG, "STUB: net_file_download_to_buffer() not implemented");
    (void)url;
    (void)buffer;
    (void)max_size;
    if (actual_size) {
        *actual_size = 0;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t net_file_upload(const char *url, const char *src_path, const char *field_name, net_file_progress_cb_t progress_cb)
{
    ESP_LOGW(TAG, "STUB: net_file_upload() not implemented");
    (void)url;
    (void)src_path;
    (void)field_name;
    (void)progress_cb;
    return ESP_ERR_NOT_SUPPORTED;
}
