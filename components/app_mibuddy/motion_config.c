/**
 * @file motion_config.c
 * @brief Motion detection threshold configuration with NVS persistence
 */

#include "motion_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "motion_cfg";

/* NVS configuration */
#define NVS_NAMESPACE       "motion_cfg"
#define NVS_KEY_MOVING      "moving_g"
#define NVS_KEY_SHAKING     "shaking_g"
#define NVS_KEY_ROTATING    "rotating"
#define NVS_KEY_SPINNING    "spinning"
#define NVS_KEY_BRAKING     "braking"

/* Scale factor for storing floats as uint32 (3 decimal places) */
#define FLOAT_SCALE         1000

/* Module state */
static struct {
    bool initialized;
    motion_config_t config;
} s_motion = {
    .initialized = false,
    .config = {
        .moving_threshold_g = MOTION_DEFAULT_MOVING_G,
        .shaking_threshold_g = MOTION_DEFAULT_SHAKING_G,
        .rotating_threshold_dps = MOTION_DEFAULT_ROTATING_DPS,
        .spinning_threshold_dps = MOTION_DEFAULT_SPINNING_DPS,
        .braking_threshold_gps = MOTION_DEFAULT_BRAKING_GPS,
    },
};

/*===========================================================================
 * Internal Helpers
 *===========================================================================*/

static uint32_t float_to_nvs(float value)
{
    return (uint32_t)(value * FLOAT_SCALE);
}

static float nvs_to_float(uint32_t value)
{
    return (float)value / FLOAT_SCALE;
}

static void load_config_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        uint32_t value;
        if (nvs_get_u32(handle, NVS_KEY_MOVING, &value) == ESP_OK) {
            s_motion.config.moving_threshold_g = nvs_to_float(value);
        }
        if (nvs_get_u32(handle, NVS_KEY_SHAKING, &value) == ESP_OK) {
            s_motion.config.shaking_threshold_g = nvs_to_float(value);
        }
        if (nvs_get_u32(handle, NVS_KEY_ROTATING, &value) == ESP_OK) {
            s_motion.config.rotating_threshold_dps = nvs_to_float(value);
        }
        if (nvs_get_u32(handle, NVS_KEY_SPINNING, &value) == ESP_OK) {
            s_motion.config.spinning_threshold_dps = nvs_to_float(value);
        }
        if (nvs_get_u32(handle, NVS_KEY_BRAKING, &value) == ESP_OK) {
            s_motion.config.braking_threshold_gps = nvs_to_float(value);
        }
        nvs_close(handle);

        ESP_LOGI(TAG, "Loaded config: moving=%.2fg, shaking=%.1fg, rotating=%.0f, spinning=%.0f, braking=%.1fg/s",
                 s_motion.config.moving_threshold_g,
                 s_motion.config.shaking_threshold_g,
                 s_motion.config.rotating_threshold_dps,
                 s_motion.config.spinning_threshold_dps,
                 s_motion.config.braking_threshold_gps);
    } else {
        ESP_LOGI(TAG, "Using defaults: moving=%.2fg, shaking=%.1fg, rotating=%.0f, spinning=%.0f, braking=%.1fg/s",
                 s_motion.config.moving_threshold_g,
                 s_motion.config.shaking_threshold_g,
                 s_motion.config.rotating_threshold_dps,
                 s_motion.config.spinning_threshold_dps,
                 s_motion.config.braking_threshold_gps);
    }
}

static esp_err_t save_single_to_nvs(const char *key, float value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u32(handle, key, float_to_nvs(value));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save %s: %s", key, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t save_all_to_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    nvs_set_u32(handle, NVS_KEY_MOVING, float_to_nvs(s_motion.config.moving_threshold_g));
    nvs_set_u32(handle, NVS_KEY_SHAKING, float_to_nvs(s_motion.config.shaking_threshold_g));
    nvs_set_u32(handle, NVS_KEY_ROTATING, float_to_nvs(s_motion.config.rotating_threshold_dps));
    nvs_set_u32(handle, NVS_KEY_SPINNING, float_to_nvs(s_motion.config.spinning_threshold_dps));
    nvs_set_u32(handle, NVS_KEY_BRAKING, float_to_nvs(s_motion.config.braking_threshold_gps));

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Saved all config to NVS");
    }
    return err;
}

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

esp_err_t motion_config_init(void)
{
    if (s_motion.initialized) {
        return ESP_OK;
    }

    /* Load config from NVS (uses defaults if not found) */
    load_config_from_nvs();

    s_motion.initialized = true;
    ESP_LOGI(TAG, "Motion config initialized");
    return ESP_OK;
}

void motion_config_deinit(void)
{
    s_motion.initialized = false;
}

/*===========================================================================
 * Configuration Access
 *===========================================================================*/

const motion_config_t* motion_config_get_ptr(void)
{
    return &s_motion.config;
}

esp_err_t motion_config_get(motion_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    *config = s_motion.config;
    return ESP_OK;
}

/*===========================================================================
 * Individual Setters
 *===========================================================================*/

esp_err_t motion_config_set_moving_threshold(float g)
{
    s_motion.config.moving_threshold_g = g;
    ESP_LOGI(TAG, "Moving threshold set to %.2f g", g);
    return save_single_to_nvs(NVS_KEY_MOVING, g);
}

esp_err_t motion_config_set_shaking_threshold(float g)
{
    s_motion.config.shaking_threshold_g = g;
    ESP_LOGI(TAG, "Shaking threshold set to %.1f g", g);
    return save_single_to_nvs(NVS_KEY_SHAKING, g);
}

esp_err_t motion_config_set_rotating_threshold(float dps)
{
    s_motion.config.rotating_threshold_dps = dps;
    ESP_LOGI(TAG, "Rotating threshold set to %.0f deg/s", dps);
    return save_single_to_nvs(NVS_KEY_ROTATING, dps);
}

esp_err_t motion_config_set_spinning_threshold(float dps)
{
    s_motion.config.spinning_threshold_dps = dps;
    ESP_LOGI(TAG, "Spinning threshold set to %.0f deg/s", dps);
    return save_single_to_nvs(NVS_KEY_SPINNING, dps);
}

esp_err_t motion_config_set_braking_threshold(float gps)
{
    s_motion.config.braking_threshold_gps = gps;
    ESP_LOGI(TAG, "Braking threshold set to %.1f g/s", gps);
    return save_single_to_nvs(NVS_KEY_BRAKING, gps);
}

/*===========================================================================
 * Individual Getters
 *===========================================================================*/

float motion_config_get_moving_threshold(void)
{
    return s_motion.config.moving_threshold_g;
}

float motion_config_get_shaking_threshold(void)
{
    return s_motion.config.shaking_threshold_g;
}

float motion_config_get_rotating_threshold(void)
{
    return s_motion.config.rotating_threshold_dps;
}

float motion_config_get_spinning_threshold(void)
{
    return s_motion.config.spinning_threshold_dps;
}

float motion_config_get_braking_threshold(void)
{
    return s_motion.config.braking_threshold_gps;
}

/*===========================================================================
 * Utilities
 *===========================================================================*/

esp_err_t motion_config_reset_defaults(void)
{
    s_motion.config.moving_threshold_g = MOTION_DEFAULT_MOVING_G;
    s_motion.config.shaking_threshold_g = MOTION_DEFAULT_SHAKING_G;
    s_motion.config.rotating_threshold_dps = MOTION_DEFAULT_ROTATING_DPS;
    s_motion.config.spinning_threshold_dps = MOTION_DEFAULT_SPINNING_DPS;
    s_motion.config.braking_threshold_gps = MOTION_DEFAULT_BRAKING_GPS;

    ESP_LOGI(TAG, "Reset to defaults");
    return save_all_to_nvs();
}
