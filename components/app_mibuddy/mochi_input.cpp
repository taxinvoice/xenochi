/**
 * @file mochi_input.cpp
 * @brief MochiState Input System Implementation
 *
 * Collects inputs from sensors/system and runs mapper function to determine state.
 */

#include "mochi_input.h"
#include "mochi_state.h"
#include "bsp_board.h"
#include "wifi_manager.h"
#include "qmi8658.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <cstring>
#include <cmath>
#include <ctime>

/* PMU access - defined in bsp_axp2101.cpp
 * CONFIG_XPOWERS_CHIP_AXP2101 is set in Kconfig, which defines XPowersPMU type
 */
#include "XPowersLib.h"
extern XPowersPMU PMU;

static const char *TAG = "mochi_input";

/*===========================================================================
 * Module State
 *===========================================================================*/

static struct {
    bool initialized;
    mochi_input_state_t state;
    mochi_mapper_fn_t mapper_fn;
    char api_url[128];
    mochi_state_t last_state;
    mochi_activity_t last_activity;
    int64_t state_change_time_us;  /* Timestamp when state last changed (microseconds) */
} s_input = {
    .initialized = false,
    .state = {},
    .mapper_fn = NULL,
    .api_url = {0},
    .last_state = MOCHI_STATE_HAPPY,
    .last_activity = MOCHI_ACTIVITY_IDLE,
    .state_change_time_us = 0,
};

/*===========================================================================
 * Async API Query System
 *
 * API queries are performed in a separate FreeRTOS task to avoid blocking
 * the LVGL task (which would trigger watchdog timeouts).
 *===========================================================================*/

static struct {
    TaskHandle_t task_handle;
    SemaphoreHandle_t mutex;
    bool query_pending;          /* Request to query API */
    bool result_ready;           /* API result is available */
    mochi_state_t result_state;
    mochi_activity_t result_activity;
    mochi_input_state_t query_input;  /* Input state to send */
} s_api = {
    .task_handle = NULL,
    .mutex = NULL,
    .query_pending = false,
    .result_ready = false,
    .result_state = MOCHI_STATE_HAPPY,
    .result_activity = MOCHI_ACTIVITY_IDLE,
    .query_input = {},
};

/*===========================================================================
 * Internal: Input Collection
 *===========================================================================*/

/**
 * @brief Collect static inputs from sensors and system
 */
static void collect_static_inputs(void)
{
    /* Battery (AXP2101) */
    s_input.state.battery_pct = (float)PMU.getBatteryPercent();
    s_input.state.is_charging = PMU.isCharging();
    s_input.state.temperature = PMU.getTemperature();

    /* Time - use system time (synced from NTP/RTC) */
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    s_input.state.hour = timeinfo.tm_hour;
    s_input.state.minute = timeinfo.tm_min;
    s_input.state.day_of_week = timeinfo.tm_wday;  /* 0=Sunday */

    /* Motion (QMI8658) */
    bsp_handles_t *handles = bsp_display_get_handles();
    if (handles && handles->qmi8658_dev) {
        qmi8658_data_t imu_data;
        bool ready = false;
        esp_err_t ret = qmi8658_is_data_ready(handles->qmi8658_dev, &ready);
        if (ret == ESP_OK && ready) {
            ret = qmi8658_read_sensor_data(handles->qmi8658_dev, &imu_data);
            if (ret == ESP_OK) {
                /* IMU is configured for m/s², convert to g-force */
                const float G = 9.807f;
                s_input.state.accel_x = imu_data.accelX / G;
                s_input.state.accel_y = imu_data.accelY / G;
                s_input.state.accel_z = imu_data.accelZ / G;
                /* Gyro is in rad/s, convert to deg/s */
                const float RAD_TO_DEG = 180.0f / 3.14159265f;
                s_input.state.gyro_x = imu_data.gyroX * RAD_TO_DEG;
                s_input.state.gyro_y = imu_data.gyroY * RAD_TO_DEG;
                s_input.state.gyro_z = imu_data.gyroZ * RAD_TO_DEG;
            }
        }
    }

    /* WiFi status */
    s_input.state.wifi_connected = wifi_manager_is_connected();

    /* Touch active - check LVGL indev state */
    bsp_handles_t *bsp = bsp_display_get_handles();
    if (bsp && bsp->lvgl_touch_indev_handle) {
        lv_indev_state_t indev_state = lv_indev_get_state(bsp->lvgl_touch_indev_handle);
        s_input.state.touch_active = (indev_state == LV_INDEV_STATE_PRESSED);
    } else {
        s_input.state.touch_active = false;
    }
}

/**
 * @brief Compute calculated/derived variables from static inputs
 */
static void compute_calculated_inputs(void)
{
    /* Battery thresholds */
    s_input.state.is_low_battery = (s_input.state.battery_pct < 20.0f);
    s_input.state.is_critical_battery = (s_input.state.battery_pct < 5.0f);

    /* Acceleration magnitude */
    float ax = s_input.state.accel_x;
    float ay = s_input.state.accel_y;
    float az = s_input.state.accel_z;
    s_input.state.accel_magnitude = sqrtf(ax*ax + ay*ay + az*az);

    /* Motion detection - deviation from rest (1g pointing down) */
    float deviation = fabsf(s_input.state.accel_magnitude - 1.0f);
    s_input.state.is_moving = (deviation > 0.3f);
    s_input.state.is_shaking = (s_input.state.accel_magnitude > 2.0f);

    /* Gyroscope magnitude and rotation detection */
    float gx = s_input.state.gyro_x;
    float gy = s_input.state.gyro_y;
    float gz = s_input.state.gyro_z;
    s_input.state.gyro_magnitude = sqrtf(gx*gx + gy*gy + gz*gz);
    s_input.state.is_rotating = (s_input.state.gyro_magnitude > 30.0f);
    s_input.state.is_spinning = (s_input.state.gyro_magnitude > 100.0f);

    /* Device orientation from gravity vector
     * Only reliable when device is relatively stationary (low gyro)
     * Threshold: 0.7g to account for tilted orientations
     */
    const float ORIENT_THRESHOLD = 0.7f;

    /* Clear all orientation flags first */
    s_input.state.is_face_up = false;
    s_input.state.is_face_down = false;
    s_input.state.is_portrait = false;
    s_input.state.is_portrait_inv = false;
    s_input.state.is_landscape_left = false;
    s_input.state.is_landscape_right = false;

    /* Find dominant axis - only set orientation if clearly dominant */
    float abs_x = fabsf(ax);
    float abs_y = fabsf(ay);
    float abs_z = fabsf(az);

    if (abs_z > abs_x && abs_z > abs_y && abs_z > ORIENT_THRESHOLD) {
        /* Z-axis dominant: face up or face down */
        if (az > 0) {
            s_input.state.is_face_up = true;
        } else {
            s_input.state.is_face_down = true;
        }
    } else if (abs_y > abs_x && abs_y > abs_z && abs_y > ORIENT_THRESHOLD) {
        /* Y-axis dominant: portrait or portrait inverted */
        if (ay < 0) {
            s_input.state.is_portrait = true;      /* Top up, gravity pulling down */
        } else {
            s_input.state.is_portrait_inv = true;  /* Top down, gravity pulling up */
        }
    } else if (abs_x > abs_y && abs_x > abs_z && abs_x > ORIENT_THRESHOLD) {
        /* X-axis dominant: landscape left or right */
        if (ax > 0) {
            s_input.state.is_landscape_left = true;   /* Left edge down */
        } else {
            s_input.state.is_landscape_right = true;  /* Right edge down */
        }
    }

    /* Tilt angles (pitch and roll) from accelerometer
     * Pitch: rotation around X-axis (forward/backward tilt)
     * Roll: rotation around Y-axis (left/right tilt)
     *
     * Using atan2 for full range and handling edge cases
     */
    const float RAD_TO_DEG = 180.0f / 3.14159265f;

    /* Pitch: angle between Z-axis and horizontal plane
     * Positive = screen facing up, Negative = screen facing down */
    s_input.state.pitch = atan2f(az, sqrtf(ax*ax + ay*ay)) * RAD_TO_DEG;

    /* Roll: rotation around the front-back axis
     * Positive = tilted right, Negative = tilted left */
    s_input.state.roll = atan2f(ax, ay) * RAD_TO_DEG;

    /* Time of day */
    int hour = s_input.state.hour;
    s_input.state.is_night = (hour >= 22 || hour < 6);

    /* Weekend check */
    int dow = s_input.state.day_of_week;
    s_input.state.is_weekend = (dow == 0 || dow == 6);  /* Sunday or Saturday */

    /* Idle detection - not moving and not rotating */
    s_input.state.is_idle = !s_input.state.is_moving && !s_input.state.is_rotating;

    /* State duration - time since last state change */
    if (s_input.state_change_time_us == 0) {
        s_input.state_change_time_us = esp_timer_get_time();
    }
    int64_t now_us = esp_timer_get_time();
    int64_t duration_us = now_us - s_input.state_change_time_us;
    s_input.state.current_state_duration_ms = (uint32_t)(duration_us / 1000);
}

/*===========================================================================
 * Async API Task
 *===========================================================================*/

/* Forward declaration of the blocking API query (used internally by async task) */
static esp_err_t api_query_blocking(
    const mochi_input_state_t *input,
    mochi_state_t *out_state,
    mochi_activity_t *out_activity);

/**
 * @brief Background task for async API queries
 *
 * Waits for query requests, performs HTTP call, stores result.
 * This runs in its own task to avoid blocking the LVGL task.
 */
static void api_query_task(void *pvParameters)
{
    ESP_LOGI(TAG, "API query task started");

    while (true) {
        /* Wait for a query request (check every 500ms) */
        vTaskDelay(pdMS_TO_TICKS(500));

        if (!s_api.query_pending) {
            continue;
        }

        /* Copy input state under mutex */
        mochi_input_state_t input_copy;
        if (xSemaphoreTake(s_api.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            input_copy = s_api.query_input;
            s_api.query_pending = false;
            xSemaphoreGive(s_api.mutex);
        } else {
            continue;  /* Couldn't get mutex, try again */
        }

        /* Perform blocking HTTP query (this is why we're in a separate task) */
        mochi_state_t state;
        mochi_activity_t activity;
        esp_err_t err = api_query_blocking(&input_copy, &state, &activity);

        /* Store result under mutex */
        if (xSemaphoreTake(s_api.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (err == ESP_OK) {
                s_api.result_state = state;
                s_api.result_activity = activity;
                s_api.result_ready = true;
                ESP_LOGI(TAG, "Async API result ready: %s + %s",
                         mochi_state_name(state), mochi_activity_name(activity));
            }
            xSemaphoreGive(s_api.mutex);
        }
    }
}

/**
 * @brief Request an async API query (non-blocking)
 *
 * Call this from the mapper. The result will be available later
 * via mochi_input_get_api_result().
 */
void mochi_input_request_api_query(const mochi_input_state_t *input)
{
    if (!s_api.mutex || !s_api.task_handle) {
        return;  /* Async system not initialized */
    }

    if (xSemaphoreTake(s_api.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (!s_api.query_pending) {  /* Don't queue multiple queries */
            s_api.query_input = *input;
            s_api.query_pending = true;
            s_api.result_ready = false;  /* Clear old result */
            ESP_LOGD(TAG, "API query requested (async)");
        }
        xSemaphoreGive(s_api.mutex);
    }
}

/**
 * @brief Check if an async API result is available
 *
 * @param out_state Output: state from API (if result ready)
 * @param out_activity Output: activity from API (if result ready)
 * @return true if result is available and was copied to outputs
 */
bool mochi_input_get_api_result(mochi_state_t *out_state, mochi_activity_t *out_activity)
{
    if (!s_api.mutex) {
        return false;
    }

    bool got_result = false;
    if (xSemaphoreTake(s_api.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (s_api.result_ready) {
            *out_state = s_api.result_state;
            *out_activity = s_api.result_activity;
            s_api.result_ready = false;  /* Consume the result */
            got_result = true;
        }
        xSemaphoreGive(s_api.mutex);
    }
    return got_result;
}

/*===========================================================================
 * Public API - Lifecycle
 *===========================================================================*/

esp_err_t mochi_input_init(void)
{
    if (s_input.initialized) {
        return ESP_OK;
    }

    memset(&s_input.state, 0, sizeof(s_input.state));
    s_input.mapper_fn = NULL;
    s_input.api_url[0] = '\0';
    s_input.last_state = MOCHI_STATE_HAPPY;
    s_input.last_activity = MOCHI_ACTIVITY_IDLE;

    /* Create async API system */
    s_api.mutex = xSemaphoreCreateMutex();
    if (s_api.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create API mutex");
        return ESP_FAIL;
    }

    BaseType_t ret = xTaskCreate(
        api_query_task,
        "mochi_api",
        4096,
        NULL,
        5,  /* Priority */
        &s_api.task_handle
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create API task");
        vSemaphoreDelete(s_api.mutex);
        s_api.mutex = NULL;
        return ESP_FAIL;
    }

    s_input.initialized = true;
    ESP_LOGI(TAG, "Mochi input system initialized (with async API)");
    return ESP_OK;
}

void mochi_input_deinit(void)
{
    /* Stop async task */
    if (s_api.task_handle) {
        vTaskDelete(s_api.task_handle);
        s_api.task_handle = NULL;
    }
    if (s_api.mutex) {
        vSemaphoreDelete(s_api.mutex);
        s_api.mutex = NULL;
    }

    s_input.initialized = false;
    s_input.mapper_fn = NULL;
    ESP_LOGI(TAG, "Mochi input system deinitialized");
}

/*===========================================================================
 * Public API - Update Loop
 *===========================================================================*/

esp_err_t mochi_input_update(void)
{
    if (!s_input.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 1. Collect static inputs */
    collect_static_inputs();

    /* 2. Compute calculated variables */
    compute_calculated_inputs();

    /* 3. Run mapper function if set */
    if (s_input.mapper_fn) {
        mochi_state_t new_state = s_input.last_state;
        mochi_activity_t new_activity = s_input.last_activity;

        s_input.mapper_fn(&s_input.state, &new_state, &new_activity);

        /* Only call mochi_set if state actually changed */
        if (new_state != s_input.last_state || new_activity != s_input.last_activity) {
            s_input.last_state = new_state;
            s_input.last_activity = new_activity;
            s_input.state_change_time_us = esp_timer_get_time();  /* Reset duration timer */
            mochi_set(new_state, new_activity);
        }
    }

    return ESP_OK;
}

const mochi_input_state_t* mochi_input_get(void)
{
    return &s_input.state;
}

/*===========================================================================
 * Public API - Mapper Configuration
 *===========================================================================*/

void mochi_input_set_mapper_fn(mochi_mapper_fn_t fn)
{
    s_input.mapper_fn = fn;
}

/*===========================================================================
 * Public API - API Helper
 *===========================================================================*/

void mochi_input_set_api_url(const char *url)
{
    if (url) {
        strncpy(s_input.api_url, url, sizeof(s_input.api_url) - 1);
        s_input.api_url[sizeof(s_input.api_url) - 1] = '\0';
    } else {
        s_input.api_url[0] = '\0';
    }
}

/**
 * @brief Parse state string to enum
 */
static mochi_state_t parse_state_string(const char *str)
{
    if (!str) return MOCHI_STATE_HAPPY;

    if (strcasecmp(str, "HAPPY") == 0) return MOCHI_STATE_HAPPY;
    if (strcasecmp(str, "EXCITED") == 0) return MOCHI_STATE_EXCITED;
    if (strcasecmp(str, "WORRIED") == 0) return MOCHI_STATE_WORRIED;
    if (strcasecmp(str, "COOL") == 0) return MOCHI_STATE_COOL;
    if (strcasecmp(str, "DIZZY") == 0) return MOCHI_STATE_DIZZY;
    if (strcasecmp(str, "PANIC") == 0) return MOCHI_STATE_PANIC;
    if (strcasecmp(str, "SLEEPY") == 0) return MOCHI_STATE_SLEEPY;
    if (strcasecmp(str, "SHOCKED") == 0) return MOCHI_STATE_SHOCKED;

    return MOCHI_STATE_HAPPY;
}

/**
 * @brief Parse activity string to enum
 */
static mochi_activity_t parse_activity_string(const char *str)
{
    if (!str) return MOCHI_ACTIVITY_IDLE;

    if (strcasecmp(str, "IDLE") == 0) return MOCHI_ACTIVITY_IDLE;
    if (strcasecmp(str, "SHAKE") == 0) return MOCHI_ACTIVITY_SHAKE;
    if (strcasecmp(str, "BOUNCE") == 0) return MOCHI_ACTIVITY_BOUNCE;
    if (strcasecmp(str, "SPIN") == 0) return MOCHI_ACTIVITY_SPIN;
    if (strcasecmp(str, "WIGGLE") == 0) return MOCHI_ACTIVITY_WIGGLE;
    if (strcasecmp(str, "NOD") == 0) return MOCHI_ACTIVITY_NOD;
    if (strcasecmp(str, "BLINK") == 0) return MOCHI_ACTIVITY_BLINK;
    if (strcasecmp(str, "SNORE") == 0) return MOCHI_ACTIVITY_SNORE;
    if (strcasecmp(str, "VIBRATE") == 0) return MOCHI_ACTIVITY_VIBRATE;

    return MOCHI_ACTIVITY_IDLE;
}

/* Buffer for HTTP response */
static char s_http_response[512];
static int s_http_response_len = 0;

/**
 * @brief HTTP event handler for API query
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (s_http_response_len + evt->data_len < sizeof(s_http_response) - 1) {
                memcpy(s_http_response + s_http_response_len, evt->data, evt->data_len);
                s_http_response_len += evt->data_len;
                s_http_response[s_http_response_len] = '\0';
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

/**
 * @brief Internal blocking API query (runs in async task)
 *
 * WARNING: This function blocks for up to 5 seconds!
 * Only call from the async API task, never from LVGL callbacks.
 */
static esp_err_t api_query_blocking(
    const mochi_input_state_t *input,
    mochi_state_t *out_state,
    mochi_activity_t *out_activity)
{
    static const char *API_TAG = "mochi_api";

    ESP_LOGI(API_TAG, "========================================");
    ESP_LOGI(API_TAG, "API QUERY START (async task)");
    ESP_LOGI(API_TAG, "========================================");

    if (!s_input.api_url[0]) {
        ESP_LOGW(API_TAG, "ABORT: API URL not set");
        return ESP_FAIL;
    }

    ESP_LOGI(API_TAG, "URL: %s", s_input.api_url);

    if (!wifi_manager_is_connected()) {
        ESP_LOGW(API_TAG, "ABORT: WiFi not connected");
        return ESP_FAIL;
    }
    ESP_LOGI(API_TAG, "WiFi: CONNECTED");

    /* Build JSON request body */
    cJSON *root = cJSON_CreateObject();

    /* Basic inputs */
    cJSON_AddNumberToObject(root, "battery", input->battery_pct);
    cJSON_AddBoolToObject(root, "charging", input->is_charging);
    cJSON_AddNumberToObject(root, "hour", input->hour);
    cJSON_AddNumberToObject(root, "minute", input->minute);
    cJSON_AddBoolToObject(root, "wifi", input->wifi_connected);
    cJSON_AddBoolToObject(root, "touch", input->touch_active);

    /* Motion detection */
    cJSON_AddBoolToObject(root, "moving", input->is_moving);
    cJSON_AddBoolToObject(root, "shaking", input->is_shaking);
    cJSON_AddBoolToObject(root, "rotating", input->is_rotating);
    cJSON_AddBoolToObject(root, "spinning", input->is_spinning);

    /* Device orientation */
    cJSON_AddBoolToObject(root, "face_up", input->is_face_up);
    cJSON_AddBoolToObject(root, "face_down", input->is_face_down);
    cJSON_AddBoolToObject(root, "portrait", input->is_portrait);
    cJSON_AddBoolToObject(root, "portrait_inv", input->is_portrait_inv);
    cJSON_AddBoolToObject(root, "landscape_left", input->is_landscape_left);
    cJSON_AddBoolToObject(root, "landscape_right", input->is_landscape_right);

    /* Tilt angles (rounded to 1 decimal) */
    cJSON_AddNumberToObject(root, "pitch", roundf(input->pitch * 10.0f) / 10.0f);
    cJSON_AddNumberToObject(root, "roll", roundf(input->roll * 10.0f) / 10.0f);

    /* Time/battery thresholds */
    cJSON_AddBoolToObject(root, "night", input->is_night);
    cJSON_AddBoolToObject(root, "weekend", input->is_weekend);
    cJSON_AddBoolToObject(root, "low_battery", input->is_low_battery);
    cJSON_AddBoolToObject(root, "critical_battery", input->is_critical_battery);

    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!post_data) {
        ESP_LOGE(API_TAG, "ABORT: Failed to create JSON request body");
        return ESP_FAIL;
    }

    /* Log the full request body */
    ESP_LOGI(API_TAG, "--- REQUEST ---");
    ESP_LOGI(API_TAG, "Method: POST");
    ESP_LOGI(API_TAG, "Content-Type: application/json");
    ESP_LOGI(API_TAG, "Body: %s", post_data);

    /* Configure HTTP client */
    esp_http_client_config_t config = {};
    config.url = s_input.api_url;
    config.method = HTTP_METHOD_POST;
    config.event_handler = http_event_handler;
    config.timeout_ms = 5000;

    ESP_LOGI(API_TAG, "Timeout: %d ms", config.timeout_ms);

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(API_TAG, "ABORT: Failed to create HTTP client");
        free(post_data);
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    /* Reset response buffer */
    s_http_response_len = 0;
    s_http_response[0] = '\0';

    /* Perform request */
    ESP_LOGI(API_TAG, "Sending HTTP request...");
    int64_t start_time = esp_timer_get_time();

    esp_err_t err = esp_http_client_perform(client);

    int64_t end_time = esp_timer_get_time();
    int64_t duration_ms = (end_time - start_time) / 1000;

    free(post_data);

    ESP_LOGI(API_TAG, "--- RESPONSE ---");
    ESP_LOGI(API_TAG, "Duration: %lld ms", duration_ms);

    if (err != ESP_OK) {
        ESP_LOGE(API_TAG, "HTTP ERROR: %s (0x%x)", esp_err_to_name(err), err);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int status_code = esp_http_client_get_status_code(client);
    int content_length = esp_http_client_get_content_length(client);
    esp_http_client_cleanup(client);

    ESP_LOGI(API_TAG, "Status: %d", status_code);
    ESP_LOGI(API_TAG, "Content-Length: %d", content_length);
    ESP_LOGI(API_TAG, "Response Body: %s", s_http_response_len > 0 ? s_http_response : "(empty)");

    if (status_code != 200) {
        ESP_LOGE(API_TAG, "FAIL: Non-200 status code");
        return ESP_FAIL;
    }

    /* Parse response JSON */
    cJSON *response = cJSON_Parse(s_http_response);
    if (!response) {
        ESP_LOGE(API_TAG, "FAIL: Invalid JSON response");
        ESP_LOGE(API_TAG, "Raw response was: '%s'", s_http_response);
        return ESP_FAIL;
    }

    ESP_LOGI(API_TAG, "--- PARSED ---");

    cJSON *state_item = cJSON_GetObjectItem(response, "state");
    cJSON *activity_item = cJSON_GetObjectItem(response, "activity");

    const char *state_str = "(not found)";
    const char *activity_str = "(not found)";

    if (cJSON_IsString(state_item)) {
        state_str = state_item->valuestring;
        *out_state = parse_state_string(state_item->valuestring);
        ESP_LOGI(API_TAG, "state: \"%s\" -> %s", state_str, mochi_state_name(*out_state));
    } else {
        ESP_LOGW(API_TAG, "state: MISSING or not a string");
    }

    if (cJSON_IsString(activity_item)) {
        activity_str = activity_item->valuestring;
        *out_activity = parse_activity_string(activity_item->valuestring);
        ESP_LOGI(API_TAG, "activity: \"%s\" -> %s", activity_str, mochi_activity_name(*out_activity));
    } else {
        ESP_LOGW(API_TAG, "activity: MISSING or not a string");
    }

    cJSON_Delete(response);

    ESP_LOGI(API_TAG, "========================================");
    ESP_LOGI(API_TAG, "API RESULT: %s + %s",
             mochi_state_name(*out_state), mochi_activity_name(*out_activity));
    ESP_LOGI(API_TAG, "========================================");

    return ESP_OK;
}
