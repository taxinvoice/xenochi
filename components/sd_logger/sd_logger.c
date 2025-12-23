/**
 * @file sd_logger.c
 * @brief SD Card Logger Implementation
 */

#include "sd_logger.h"
#include "sd_file.h"
#include "bsp_board.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <dirent.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "sd_logger";

/* NVS namespace and keys */
#define NVS_NAMESPACE "sd_logger"
#define NVS_KEY_ENABLED "enabled"

/* Default configuration from Kconfig */
#ifndef CONFIG_SD_LOGGER_MAX_FILE_SIZE_KB
#define CONFIG_SD_LOGGER_MAX_FILE_SIZE_KB 1024
#endif

#ifndef CONFIG_SD_LOGGER_MAX_FILES
#define CONFIG_SD_LOGGER_MAX_FILES 5
#endif

#ifndef CONFIG_SD_LOGGER_DIRECTORY
#define CONFIG_SD_LOGGER_DIRECTORY "/sdcard/logs"
#endif

#ifndef CONFIG_SD_LOGGER_DEFAULT_ENABLED
#define CONFIG_SD_LOGGER_DEFAULT_ENABLED 0
#endif

/* Module state */
static sd_logger_config_t s_config = {
    .enabled = false,
    .max_file_size_kb = CONFIG_SD_LOGGER_MAX_FILE_SIZE_KB,
    .max_files = CONFIG_SD_LOGGER_MAX_FILES,
    .log_dir = CONFIG_SD_LOGGER_DIRECTORY
};

static bool s_initialized = false;
static FILE *s_log_file = NULL;
static char s_current_file_path[128] = {0};
static vprintf_like_t s_original_vprintf = NULL;
static SemaphoreHandle_t s_mutex = NULL;
static StaticSemaphore_t s_mutex_buffer;

/* Forward declarations */
static int sd_logger_vprintf(const char *fmt, va_list args);
static void rotate_log_files(void);
static void open_log_file(void);
static void close_log_file(void);

/**
 * @brief Take mutex with timeout
 */
static bool take_mutex(void)
{
    if (s_mutex == NULL) {
        return false;
    }
    return xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) == pdTRUE;
}

/**
 * @brief Release mutex
 */
static void give_mutex(void)
{
    if (s_mutex != NULL) {
        xSemaphoreGive(s_mutex);
    }
}

/**
 * @brief Load enabled state from NVS
 */
static void load_enabled_from_nvs(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        uint8_t enabled = 0;
        err = nvs_get_u8(nvs, NVS_KEY_ENABLED, &enabled);
        if (err == ESP_OK) {
            s_config.enabled = (enabled != 0);
        } else {
            s_config.enabled = CONFIG_SD_LOGGER_DEFAULT_ENABLED;
        }
        nvs_close(nvs);
    } else {
        s_config.enabled = CONFIG_SD_LOGGER_DEFAULT_ENABLED;
    }
}

/**
 * @brief Save enabled state to NVS
 */
static esp_err_t save_enabled_to_nvs(bool enabled)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u8(nvs, NVS_KEY_ENABLED, enabled ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);

    return err;
}

/**
 * @brief Get current timestamp string
 */
static void get_timestamp(char *buf, size_t buf_len)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(buf, buf_len, "[%Y-%m-%d %H:%M:%S] ", &timeinfo);
}

/**
 * @brief Custom vprintf that logs to both console and SD card
 */
static int sd_logger_vprintf(const char *fmt, va_list args)
{
    /* Always output to console first */
    va_list args_copy;
    va_copy(args_copy, args);
    int ret = vprintf(fmt, args_copy);
    va_end(args_copy);

    /* Only log to SD if enabled and file is open */
    if (s_config.enabled && s_log_file != NULL) {
        if (take_mutex()) {
            /* Check if we need to rotate */
            long file_pos = ftell(s_log_file);
            if (file_pos > 0 && (uint32_t)file_pos >= s_config.max_file_size_kb * 1024) {
                rotate_log_files();
                open_log_file();
            }

            if (s_log_file != NULL) {
                /* Add timestamp at start of each log line */
                static bool at_line_start = true;
                if (at_line_start) {
                    char timestamp[32];
                    get_timestamp(timestamp, sizeof(timestamp));
                    fputs(timestamp, s_log_file);
                }

                /* Write the log message */
                va_list args_file;
                va_copy(args_file, args);
                vfprintf(s_log_file, fmt, args_file);
                va_end(args_file);

                /* Check if message ends with newline */
                size_t fmt_len = strlen(fmt);
                at_line_start = (fmt_len > 0 && fmt[fmt_len - 1] == '\n');

                /* Flush periodically to ensure data is written */
                fflush(s_log_file);
            }
            give_mutex();
        }
    }

    return ret;
}

/**
 * @brief Generate current log file path
 */
static void generate_log_file_path(void)
{
    snprintf(s_current_file_path, sizeof(s_current_file_path),
             "%s/system.log", s_config.log_dir);
}

/**
 * @brief Open the current log file for appending
 */
static void open_log_file(void)
{
    if (s_log_file != NULL) {
        fclose(s_log_file);
        s_log_file = NULL;
    }

    generate_log_file_path();
    s_log_file = fopen(s_current_file_path, "a");
    if (s_log_file == NULL) {
        ESP_LOGE(TAG, "Failed to open log file: %s", s_current_file_path);
    }
}

/**
 * @brief Close the current log file
 */
static void close_log_file(void)
{
    if (s_log_file != NULL) {
        fflush(s_log_file);
        fclose(s_log_file);
        s_log_file = NULL;
    }
}

/**
 * @brief Rotate log files - delete current and start fresh
 */
static void rotate_log_files(void)
{
    close_log_file();

    /* Simply delete the current log file and start fresh */
    char path[140];
    snprintf(path, sizeof(path), "%s/system.log", s_config.log_dir);
    if (sd_file_exists(path)) {
        sd_file_delete(path);
    }

    ESP_LOGI(TAG, "Log file rotated (new file created)");
}

esp_err_t sd_logger_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    /* Create mutex */
    s_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buffer);
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }

    /* Load enabled state from NVS */
    load_enabled_from_nvs();

    /* Create log directory */
    esp_err_t ret = sd_file_mkdir(s_config.log_dir);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to create log directory (may already exist)");
    }

    /* Hook into ESP-IDF logging */
    s_original_vprintf = esp_log_set_vprintf(sd_logger_vprintf);

    /* Open log file if enabled */
    if (s_config.enabled) {
        open_log_file();
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Initialized (enabled=%d, max_size=%luKB, max_files=%d)",
             s_config.enabled, s_config.max_file_size_kb, s_config.max_files);

    return ESP_OK;
}

esp_err_t sd_logger_set_enabled(bool enabled)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_FAIL;
    }

    if (s_config.enabled == enabled) {
        return ESP_OK;
    }

    if (!take_mutex()) {
        return ESP_FAIL;
    }

    s_config.enabled = enabled;

    if (enabled) {
        open_log_file();
        ESP_LOGI(TAG, "Logging enabled");
    } else {
        close_log_file();
        ESP_LOGI(TAG, "Logging disabled");
    }

    give_mutex();

    /* Save to NVS */
    return save_enabled_to_nvs(enabled);
}

bool sd_logger_is_enabled(void)
{
    return s_config.enabled;
}

const char* sd_logger_get_current_file(void)
{
    if (!s_initialized) {
        return NULL;
    }
    return s_current_file_path;
}

uint32_t sd_logger_get_total_size_kb(void)
{
    char path[140];
    snprintf(path, sizeof(path), "%s/system.log", s_config.log_dir);
    int32_t size = sd_file_size(path);
    return (size > 0) ? (size / 1024) : 0;
}

uint8_t sd_logger_get_file_count(void)
{
    char path[140];
    snprintf(path, sizeof(path), "%s/system.log", s_config.log_dir);
    return sd_file_exists(path) ? 1 : 0;
}

esp_err_t sd_logger_clear_all(void)
{
    if (!s_initialized) {
        return ESP_FAIL;
    }

    if (!take_mutex()) {
        return ESP_FAIL;
    }

    /* Close current file first */
    close_log_file();

    /* Delete log file */
    char path[140];
    snprintf(path, sizeof(path), "%s/system.log", s_config.log_dir);
    sd_file_delete(path);

    /* Reopen log file if enabled */
    if (s_config.enabled) {
        open_log_file();
    }

    give_mutex();

    ESP_LOGI(TAG, "Log file cleared");
    return ESP_OK;
}

void sd_logger_flush(void)
{
    if (s_log_file != NULL && take_mutex()) {
        fflush(s_log_file);
        give_mutex();
    }
}

void sd_logger_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    if (take_mutex()) {
        /* Restore original vprintf */
        if (s_original_vprintf != NULL) {
            esp_log_set_vprintf(s_original_vprintf);
            s_original_vprintf = NULL;
        }

        /* Close log file */
        close_log_file();

        give_mutex();
    }

    s_initialized = false;
    ESP_LOGI(TAG, "Deinitialized");
}

esp_err_t sd_logger_get_config(sd_logger_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(config, &s_config, sizeof(sd_logger_config_t));
    return ESP_OK;
}
