/**
 * @file power_manager.c
 * @brief Power management implementation for face-down sleep mode
 */

#include "power_manager.h"
#include "mochi_input.h"
#include "bsp_board.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"

static const char *TAG = "power_mgr";

/* NVS configuration */
#define NVS_NAMESPACE       "power_mgr"
#define NVS_KEY_SCREEN_OFF  "scrn_off"
#define NVS_KEY_SLEEP       "sleep_sec"
#define NVS_KEY_IDLE_OFF    "idle_off"

/* Task configuration */
#define POWER_TASK_STACK_SIZE   4096
#define POWER_TASK_PRIORITY     5
#define POWER_TASK_POLL_MS      1000    /* Check state every 1 second */

/* Fade duration */
#define SCREEN_FADE_OFF_MS      3000    /* 3 second fade to off */
#define SCREEN_FADE_ON_MS       500     /* 0.5 second fade on wake */

/* Internal state */
static struct {
    power_state_t state;
    int64_t face_down_start_us;         /* When face-down started (0 = not face down) */
    int64_t screen_off_start_us;        /* When screen turned off */
    int64_t last_activity_us;           /* Last touch/motion timestamp */
    bool sleep_inhibited;
    uint8_t saved_backlight;            /* Backlight level before turning off */
    power_manager_config_t config;
    TaskHandle_t task_handle;
    bool initialized;
    bool running;
} s_pm = {
    .state = POWER_STATE_ACTIVE,
    .face_down_start_us = 0,
    .screen_off_start_us = 0,
    .last_activity_us = 0,
    .sleep_inhibited = false,
    .saved_backlight = DEFAULT_BACKLIGHT,
    .config = {
        .screen_off_timeout_sec = CONFIG_POWER_SCREEN_OFF_TIMEOUT_SEC,
        .light_sleep_timeout_sec = CONFIG_POWER_LIGHT_SLEEP_TIMEOUT_SEC,
        .idle_screen_off_timeout_sec = CONFIG_POWER_IDLE_SCREEN_OFF_TIMEOUT_SEC,
    },
    .task_handle = NULL,
    .initialized = false,
    .running = false,
};

/* State change callback */
static power_state_cb_t s_state_callback = NULL;

/* Forward declarations */
static void power_manager_task(void *arg);
static void load_config_from_nvs(void);
static void save_config_to_nvs(void);
static void transition_to_screen_off(void);
static void transition_to_light_sleep(void);
static void transition_to_active(void);
static void configure_wake_sources(void);

/*===========================================================================
 * NVS Configuration
 *===========================================================================*/

static void load_config_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        uint32_t value;
        if (nvs_get_u32(handle, NVS_KEY_SCREEN_OFF, &value) == ESP_OK) {
            s_pm.config.screen_off_timeout_sec = value;
        }
        if (nvs_get_u32(handle, NVS_KEY_SLEEP, &value) == ESP_OK) {
            s_pm.config.light_sleep_timeout_sec = value;
        }
        if (nvs_get_u32(handle, NVS_KEY_IDLE_OFF, &value) == ESP_OK) {
            s_pm.config.idle_screen_off_timeout_sec = value;
        }
        nvs_close(handle);
        ESP_LOGI(TAG, "Loaded config: screen_off=%lus, sleep=%lus, idle=%lus",
                 s_pm.config.screen_off_timeout_sec,
                 s_pm.config.light_sleep_timeout_sec,
                 s_pm.config.idle_screen_off_timeout_sec);
    } else {
        ESP_LOGI(TAG, "Using default config: screen_off=%lus, sleep=%lus, idle=%lus",
                 s_pm.config.screen_off_timeout_sec,
                 s_pm.config.light_sleep_timeout_sec,
                 s_pm.config.idle_screen_off_timeout_sec);
    }
}

static void save_config_to_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_u32(handle, NVS_KEY_SCREEN_OFF, s_pm.config.screen_off_timeout_sec);
        nvs_set_u32(handle, NVS_KEY_SLEEP, s_pm.config.light_sleep_timeout_sec);
        nvs_set_u32(handle, NVS_KEY_IDLE_OFF, s_pm.config.idle_screen_off_timeout_sec);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Saved config to NVS");
    } else {
        ESP_LOGE(TAG, "Failed to save config: %s", esp_err_to_name(err));
    }
}

/*===========================================================================
 * State Transitions
 *===========================================================================*/

static void transition_to_screen_off(void)
{
    if (s_pm.state == POWER_STATE_SCREEN_OFF) return;

    ESP_LOGI(TAG, "Transitioning to SCREEN_OFF");

    power_state_t old_state = s_pm.state;

    /* Save current backlight level */
    s_pm.saved_backlight = bsp_read_backlight_value();
    if (s_pm.saved_backlight == 0) {
        s_pm.saved_backlight = DEFAULT_BACKLIGHT;
    }

    /* Fade screen to off */
    bsp_fade_backlight(0, SCREEN_FADE_OFF_MS);

    s_pm.state = POWER_STATE_SCREEN_OFF;
    s_pm.screen_off_start_us = esp_timer_get_time();

    if (s_state_callback) {
        s_state_callback(old_state, POWER_STATE_SCREEN_OFF);
    }
}

static void transition_to_light_sleep(void)
{
    if (s_pm.state == POWER_STATE_LIGHT_SLEEP) return;
    if (s_pm.sleep_inhibited) {
        ESP_LOGD(TAG, "Sleep inhibited, staying in SCREEN_OFF");
        return;
    }

    ESP_LOGI(TAG, "Transitioning to LIGHT_SLEEP");

    power_state_t old_state = s_pm.state;
    s_pm.state = POWER_STATE_LIGHT_SLEEP;

    if (s_state_callback) {
        s_state_callback(old_state, POWER_STATE_LIGHT_SLEEP);
    }

    /* Configure wake sources */
    configure_wake_sources();

    /* Enter light sleep loop - will wake periodically to check motion */
    while (s_pm.state == POWER_STATE_LIGHT_SLEEP && s_pm.running) {
        esp_light_sleep_start();

        esp_sleep_wakeup_cause_t wake_cause = esp_sleep_get_wakeup_cause();
        ESP_LOGD(TAG, "Woke from light sleep, cause: %d", wake_cause);

        if (wake_cause == ESP_SLEEP_WAKEUP_GPIO) {
            /* Touch wake - return to active */
            ESP_LOGI(TAG, "Touch wake detected");
            transition_to_active();
            break;
        } else if (wake_cause == ESP_SLEEP_WAKEUP_TIMER) {
            /* Timer wake - check if still face-down */
            const mochi_input_state_t *input = mochi_input_get();
            if (input && !input->is_face_down) {
                ESP_LOGI(TAG, "Motion wake - no longer face-down");
                transition_to_active();
                break;
            }
            /* Still face-down, go back to sleep */
        } else {
            /* Unknown wake - return to active for safety */
            ESP_LOGW(TAG, "Unknown wake cause: %d", wake_cause);
            transition_to_active();
            break;
        }
    }
}

static void transition_to_active(void)
{
    if (s_pm.state == POWER_STATE_ACTIVE) return;

    ESP_LOGI(TAG, "Transitioning to ACTIVE");

    power_state_t old_state = s_pm.state;

    /* Disable wake sources */
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    /* Restore backlight with fade */
    bsp_fade_backlight(s_pm.saved_backlight, SCREEN_FADE_ON_MS);

    s_pm.state = POWER_STATE_ACTIVE;
    s_pm.face_down_start_us = 0;
    s_pm.screen_off_start_us = 0;
    s_pm.last_activity_us = esp_timer_get_time();

    if (s_state_callback) {
        s_state_callback(old_state, POWER_STATE_ACTIVE);
    }
}

static void configure_wake_sources(void)
{
    /* Configure touch interrupt as wake source (GPIO 11, active low) */
    gpio_wakeup_enable(TOUCH_INT, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    /* Configure timer wake for motion polling */
    esp_sleep_enable_timer_wakeup(CONFIG_POWER_MOTION_POLL_INTERVAL_MS * 1000ULL);

    ESP_LOGD(TAG, "Wake sources configured: GPIO %d + timer %d ms",
             TOUCH_INT, CONFIG_POWER_MOTION_POLL_INTERVAL_MS);
}

/*===========================================================================
 * Power Manager Task
 *===========================================================================*/

static void power_manager_task(void *arg)
{
    ESP_LOGI(TAG, "Power manager task started");

    while (s_pm.running) {
        /* Get current input state */
        const mochi_input_state_t *input = mochi_input_get();
        if (!input) {
            vTaskDelay(pdMS_TO_TICKS(POWER_TASK_POLL_MS));
            continue;
        }

        int64_t now_us = esp_timer_get_time();

        /* Track activity - reset idle timer on touch or motion */
        if (input->touch_active || input->is_moving) {
            s_pm.last_activity_us = now_us;
        }

        switch (s_pm.state) {
            case POWER_STATE_ACTIVE: {
                bool should_screen_off = false;

                /* Face-down trigger */
                if (input->is_face_down) {
                    if (s_pm.face_down_start_us == 0) {
                        s_pm.face_down_start_us = now_us;
                        ESP_LOGD(TAG, "Face-down detected");
                    } else {
                        int64_t face_down_sec = (now_us - s_pm.face_down_start_us) / 1000000;
                        if (face_down_sec >= s_pm.config.screen_off_timeout_sec) {
                            ESP_LOGI(TAG, "Face-down timeout reached");
                            should_screen_off = true;
                        }
                    }
                } else {
                    if (s_pm.face_down_start_us != 0) {
                        ESP_LOGD(TAG, "No longer face-down");
                    }
                    s_pm.face_down_start_us = 0;
                }

                /* Idle trigger (uses separate timeout) */
                if (!should_screen_off && s_pm.config.idle_screen_off_timeout_sec > 0 &&
                    s_pm.last_activity_us > 0) {
                    int64_t idle_sec = (now_us - s_pm.last_activity_us) / 1000000;
                    if (idle_sec >= s_pm.config.idle_screen_off_timeout_sec) {
                        ESP_LOGI(TAG, "Idle timeout reached (%lld sec)", idle_sec);
                        should_screen_off = true;
                    }
                }

                if (should_screen_off) {
                    transition_to_screen_off();
                }
                break;
            }

            case POWER_STATE_SCREEN_OFF:
                /* Wake on activity (touch or motion) */
                if (input->touch_active || input->is_moving) {
                    ESP_LOGI(TAG, "Activity detected, waking up");
                    transition_to_active();
                } else if (!input->is_face_down && s_pm.face_down_start_us != 0) {
                    /* Was face-down, now picked up */
                    ESP_LOGI(TAG, "Device picked up");
                    transition_to_active();
                } else {
                    /* Check if light-sleep timeout reached (shared timeout) */
                    int64_t screen_off_sec = (now_us - s_pm.screen_off_start_us) / 1000000;
                    int64_t sleep_delay = s_pm.config.light_sleep_timeout_sec -
                                          s_pm.config.screen_off_timeout_sec;
                    if (sleep_delay > 0 && screen_off_sec >= sleep_delay) {
                        transition_to_light_sleep();
                    }
                }
                break;

            case POWER_STATE_LIGHT_SLEEP:
                /* Handled in transition_to_light_sleep() loop */
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(POWER_TASK_POLL_MS));
    }

    ESP_LOGI(TAG, "Power manager task stopped");
    vTaskDelete(NULL);
}

/*===========================================================================
 * Public API
 *===========================================================================*/

esp_err_t power_manager_init(void)
{
    if (s_pm.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    /* Load configuration from NVS */
    load_config_from_nvs();

    /* Initialize activity timer */
    s_pm.last_activity_us = esp_timer_get_time();

    /* Start power manager task */
    s_pm.running = true;
    BaseType_t ret = xTaskCreate(
        power_manager_task,
        "power_mgr",
        POWER_TASK_STACK_SIZE,
        NULL,
        POWER_TASK_PRIORITY,
        &s_pm.task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        return ESP_FAIL;
    }

    s_pm.initialized = true;
    ESP_LOGI(TAG, "Initialized");
    return ESP_OK;
}

void power_manager_deinit(void)
{
    if (!s_pm.initialized) return;

    s_pm.running = false;

    /* Wait for task to stop */
    if (s_pm.task_handle) {
        vTaskDelay(pdMS_TO_TICKS(POWER_TASK_POLL_MS * 2));
        s_pm.task_handle = NULL;
    }

    /* Ensure screen is on */
    if (s_pm.state != POWER_STATE_ACTIVE) {
        transition_to_active();
    }

    s_pm.initialized = false;
    ESP_LOGI(TAG, "Deinitialized");
}

power_state_t power_manager_get_state(void)
{
    return s_pm.state;
}

esp_err_t power_manager_get_config(power_manager_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    *config = s_pm.config;
    return ESP_OK;
}

esp_err_t power_manager_set_screen_off_timeout(uint32_t seconds)
{
    if (seconds < 10 || seconds > 600) {
        return ESP_ERR_INVALID_ARG;
    }
    s_pm.config.screen_off_timeout_sec = seconds;
    save_config_to_nvs();
    return ESP_OK;
}

esp_err_t power_manager_set_sleep_timeout(uint32_t seconds)
{
    if (seconds < 60 || seconds > 1800) {
        return ESP_ERR_INVALID_ARG;
    }
    s_pm.config.light_sleep_timeout_sec = seconds;
    save_config_to_nvs();
    return ESP_OK;
}

uint32_t power_manager_get_screen_off_timeout(void)
{
    return s_pm.config.screen_off_timeout_sec;
}

uint32_t power_manager_get_sleep_timeout(void)
{
    return s_pm.config.light_sleep_timeout_sec;
}

void power_manager_inhibit_sleep(bool inhibit)
{
    s_pm.sleep_inhibited = inhibit;
    ESP_LOGI(TAG, "Sleep %s", inhibit ? "inhibited" : "allowed");
}

bool power_manager_is_sleep_inhibited(void)
{
    return s_pm.sleep_inhibited;
}

void power_manager_wake(void)
{
    if (s_pm.state != POWER_STATE_ACTIVE) {
        ESP_LOGI(TAG, "Manual wake requested");
        transition_to_active();
    }
}

esp_err_t power_manager_set_idle_timeout(uint32_t seconds)
{
    if (seconds != 0 && (seconds < 60 || seconds > 1800)) {
        return ESP_ERR_INVALID_ARG;
    }
    s_pm.config.idle_screen_off_timeout_sec = seconds;
    save_config_to_nvs();
    ESP_LOGI(TAG, "Idle timeout set to %lu sec", seconds);
    return ESP_OK;
}

uint32_t power_manager_get_idle_timeout(void)
{
    return s_pm.config.idle_screen_off_timeout_sec;
}

void power_manager_register_callback(power_state_cb_t cb)
{
    s_state_callback = cb;
    ESP_LOGI(TAG, "State callback %s", cb ? "registered" : "unregistered");
}
