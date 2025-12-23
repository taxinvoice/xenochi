/**
 * @file sd_file.c
 * @brief SD Card File Operations Implementation
 */

#include "sd_file.h"
#include "bsp_board.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "sd_file";

/* Mutex for thread safety */
static SemaphoreHandle_t s_file_mutex = NULL;
static StaticSemaphore_t s_mutex_buffer;

/**
 * @brief Initialize mutex if not already done
 */
static void ensure_mutex_init(void)
{
    if (s_file_mutex == NULL) {
        s_file_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buffer);
    }
}

/**
 * @brief Take mutex with timeout
 */
static bool take_mutex(void)
{
    ensure_mutex_init();
    return xSemaphoreTake(s_file_mutex, pdMS_TO_TICKS(5000)) == pdTRUE;
}

/**
 * @brief Release mutex
 */
static void give_mutex(void)
{
    if (s_file_mutex != NULL) {
        xSemaphoreGive(s_file_mutex);
    }
}

esp_err_t sd_file_write(const char *path, const void *data, size_t len)
{
    if (path == NULL || data == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    if (!take_mutex()) {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_FAIL;
    }

    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s (errno=%d)", path, errno);
        give_mutex();
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    give_mutex();

    if (written != len) {
        ESP_LOGE(TAG, "Write incomplete: %zu of %zu bytes", written, len);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Wrote %zu bytes to %s", len, path);
    return ESP_OK;
}

esp_err_t sd_file_write_string(const char *path, const char *str)
{
    if (str == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return sd_file_write(path, str, strlen(str));
}

esp_err_t sd_file_append(const char *path, const void *data, size_t len)
{
    if (path == NULL || data == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    if (!take_mutex()) {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_FAIL;
    }

    FILE *f = fopen(path, "ab");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for appending: %s (errno=%d)", path, errno);
        give_mutex();
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    give_mutex();

    if (written != len) {
        ESP_LOGE(TAG, "Append incomplete: %zu of %zu bytes", written, len);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Appended %zu bytes to %s", len, path);
    return ESP_OK;
}

esp_err_t sd_file_append_string(const char *path, const char *str)
{
    if (str == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return sd_file_append(path, str, strlen(str));
}

int sd_file_read(const char *path, void *buf, size_t buf_len)
{
    if (path == NULL || buf == NULL || buf_len == 0) {
        ESP_LOGE(TAG, "Invalid arguments");
        return -1;
    }

    if (!take_mutex()) {
        ESP_LOGE(TAG, "Failed to take mutex");
        return -1;
    }

    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s (errno=%d)", path, errno);
        give_mutex();
        return -1;
    }

    size_t bytes_read = fread(buf, 1, buf_len, f);
    fclose(f);
    give_mutex();

    ESP_LOGD(TAG, "Read %zu bytes from %s", bytes_read, path);
    return (int)bytes_read;
}

esp_err_t sd_file_delete(const char *path)
{
    if (path == NULL) {
        ESP_LOGE(TAG, "Invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

    if (!take_mutex()) {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_FAIL;
    }

    int ret = unlink(path);
    give_mutex();

    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to delete file: %s (errno=%d)", path, errno);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Deleted file: %s", path);
    return ESP_OK;
}

bool sd_file_exists(const char *path)
{
    if (path == NULL) {
        return false;
    }

    struct stat st;
    return (stat(path, &st) == 0);
}

int32_t sd_file_size(const char *path)
{
    if (path == NULL) {
        return -1;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }

    return (int32_t)st.st_size;
}

esp_err_t sd_file_mkdir(const char *path)
{
    if (path == NULL) {
        ESP_LOGE(TAG, "Invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

    /* Check if directory already exists */
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            ESP_LOGD(TAG, "Directory already exists: %s", path);
            return ESP_OK;
        }
        ESP_LOGE(TAG, "Path exists but is not a directory: %s", path);
        return ESP_FAIL;
    }

    if (!take_mutex()) {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_FAIL;
    }

    /* Create directory with parent directories */
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);

    /* Remove trailing slash */
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    /* Create each directory in path */
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (stat(tmp, &st) != 0) {
                if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                    ESP_LOGE(TAG, "Failed to create directory: %s (errno=%d)", tmp, errno);
                    give_mutex();
                    return ESP_FAIL;
                }
            }
            *p = '/';
        }
    }

    /* Create final directory */
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        ESP_LOGE(TAG, "Failed to create directory: %s (errno=%d)", tmp, errno);
        give_mutex();
        return ESP_FAIL;
    }

    give_mutex();
    ESP_LOGD(TAG, "Created directory: %s", path);
    return ESP_OK;
}

esp_err_t sd_file_rename(const char *old_path, const char *new_path)
{
    if (old_path == NULL || new_path == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    if (!take_mutex()) {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_FAIL;
    }

    /* Try native rename first */
    int ret = rename(old_path, new_path);
    if (ret == 0) {
        give_mutex();
        ESP_LOGD(TAG, "Renamed %s to %s", old_path, new_path);
        return ESP_OK;
    }

    /* FAT filesystem rename often fails - fall back to copy and delete */
    ESP_LOGD(TAG, "rename() failed (errno=%d), using copy fallback", errno);

    /* Delete destination if it exists */
    unlink(new_path);

    /* Open source file for reading */
    FILE *src = fopen(old_path, "rb");
    if (src == NULL) {
        ESP_LOGE(TAG, "Failed to open source file: %s (errno=%d)", old_path, errno);
        give_mutex();
        return ESP_FAIL;
    }

    /* Open destination file for writing */
    FILE *dst = fopen(new_path, "wb");
    if (dst == NULL) {
        ESP_LOGE(TAG, "Failed to open destination file: %s (errno=%d)", new_path, errno);
        fclose(src);
        give_mutex();
        return ESP_FAIL;
    }

    /* Copy in chunks */
    static char copy_buf[512];
    size_t bytes_read;
    esp_err_t result = ESP_OK;

    while ((bytes_read = fread(copy_buf, 1, sizeof(copy_buf), src)) > 0) {
        if (fwrite(copy_buf, 1, bytes_read, dst) != bytes_read) {
            ESP_LOGE(TAG, "Write error during copy");
            result = ESP_FAIL;
            break;
        }
    }

    fclose(src);
    fclose(dst);

    if (result == ESP_OK) {
        /* Delete original file */
        unlink(old_path);
        ESP_LOGD(TAG, "Renamed %s to %s (via copy)", old_path, new_path);
    } else {
        /* Clean up failed copy */
        unlink(new_path);
    }

    give_mutex();
    return result;
}
