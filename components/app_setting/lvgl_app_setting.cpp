/**
 * @file lvgl_app_setting.cpp
 * @brief Settings Application for ESP-Brookesia Phone UI
 *
 * This application provides system settings and diagnostics:
 * - SD card storage information
 * - Flash memory size
 * - Real-time clock display (PCF85063A)
 * - Battery/power status (AXP2101 PMU)
 * - WiFi network scanning
 * - Backlight brightness control
 *
 * UI Layout:
 * - Main panel with grid layout for settings items
 * - Popup message boxes for detailed battery and WiFi info
 * - Slider for backlight adjustment
 *
 * Lifecycle:
 * - run(): Creates settings UI with all controls
 * - back(): Closes app and returns to home
 * - close(): Cleanup (stops timers)
 */

#include "lvgl_app_setting.hpp"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "private/esp_brookesia_utils.h"

#include "app_setting_assets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_flash.h"
#include "bsp_board.h"
#include "XPowersLib.h"
#include "wifi_scan.h"
#include "wifi_manager.h"
#include "time_sync.h"
#include "sd_logger.h"
#include "power_manager.h"
#include "motion_config.h"

using namespace std;
using namespace esp_brookesia::gui;

/*===========================================================================
 * UI Element References
 *===========================================================================*/

static lv_style_t style_text_muted;         /**< Muted text style */
static lv_style_t style_textarea_placeholder; /**< Dark placeholder text style */

/* Main panel elements */
static lv_obj_t * SD_Size;                  /**< SD card size text area */
static lv_obj_t * FlashSize;                /**< Flash size text area */
static lv_obj_t * RTC_Time;                 /**< RTC time display */
static lv_obj_t * Last_NTP_Sync;            /**< Last NTP sync time display */
static lv_obj_t * but_ntp_sync;             /**< NTP sync button */
static lv_obj_t * but_bat_msg;              /**< Battery info button */
static lv_obj_t * but_wifi_msg;             /**< WiFi scan button */
static lv_obj_t * Backlight_slider;         /**< Backlight brightness slider */
static lv_timer_t * auto_step_timer = NULL; /**< RTC update timer */

/* Battery message box elements */
static lv_obj_t *list_bat_msg;              /**< Battery info list */
static lv_obj_t *label_charging;            /**< Charging status label */
static lv_obj_t *label_battery_connect;     /**< Battery connected label */
static lv_obj_t *label_vbus_in;             /**< VBUS input label */
static lv_obj_t *label_battery_percent;     /**< Battery percentage label */
static lv_obj_t *label_battery_voltage;     /**< Battery voltage label */
static lv_obj_t *label_vbus_voltage;        /**< VBUS voltage label */
static lv_obj_t *label_system_voltage;      /**< System voltage label */
static lv_obj_t *label_dc1_voltage;         /**< DC1 rail voltage label */
static lv_obj_t *label_aldo1_voltage;       /**< ALDO1 voltage label */
static lv_obj_t *label_bldo1_voltage;       /**< BLDO1 voltage label */
static lv_obj_t *label_bldo2_voltage;       /**< BLDO2 voltage label */
static lv_timer_t* axp_timer = NULL;        /**< Battery status update timer */

/* File logging UI elements */
static lv_obj_t *logging_switch = NULL;     /**< Logging enable/disable switch */
static lv_obj_t *logging_size_label = NULL; /**< Log size display label */
static lv_obj_t *but_clear_logs = NULL;     /**< Clear logs button */

/* Sleep settings UI elements */
static lv_obj_t *screen_off_slider = NULL;  /**< Screen off timeout slider */
static lv_obj_t *screen_off_value = NULL;   /**< Screen off timeout value label */
static lv_obj_t *sleep_slider = NULL;       /**< Light sleep timeout slider */
static lv_obj_t *sleep_value = NULL;        /**< Light sleep timeout value label */

/* Motion settings UI elements */
static lv_obj_t *motion_moving_slider = NULL;
static lv_obj_t *motion_moving_value = NULL;
static lv_obj_t *motion_shaking_slider = NULL;
static lv_obj_t *motion_shaking_value = NULL;
static lv_obj_t *motion_rotating_slider = NULL;
static lv_obj_t *motion_rotating_value = NULL;
static lv_obj_t *motion_spinning_slider = NULL;
static lv_obj_t *motion_spinning_value = NULL;
static lv_obj_t *motion_braking_slider = NULL;
static lv_obj_t *motion_braking_value = NULL;
static lv_obj_t *motion_reset_btn = NULL;

/* External PMU object from AXP2101 driver */
extern XPowersPMU PMU;

/* Forward declaration */
static void example1_increase_lvgl_tick(lv_timer_t * t);


/*===========================================================================
 * Constructors/Destructor
 *===========================================================================*/

/**
 * @brief Construct settings app with status/navigation bar options
 */
PhoneSettingConf::PhoneSettingConf(bool use_status_bar, bool use_navigation_bar):
    ESP_Brookesia_PhoneApp("Setting", &icon_setting, true, use_status_bar, use_navigation_bar)
{
}

/**
 * @brief Construct settings app with default settings
 */
PhoneSettingConf::PhoneSettingConf():
    ESP_Brookesia_PhoneApp("Setting", &icon_setting, true)
{
}

/**
 * @brief Destructor
 */
PhoneSettingConf::~PhoneSettingConf()
{
    ESP_BROOKESIA_LOGD("Destroy(@0x%p)", this);
}

/*===========================================================================
 * Event Callbacks
 *===========================================================================*/

/**
 * @brief Backlight slider change callback
 *
 * Called when user adjusts the backlight slider.
 * Updates both the slider display and the actual LCD backlight.
 *
 * @param e LVGL event
 */
static void Backlight_adjustment_event_cb(lv_event_t * e)
{
    uint8_t Backlight = lv_slider_get_value((lv_obj_t*)lv_event_get_target(e));
    if (Backlight <= Backlight_MAX) {
        lv_slider_set_value(Backlight_slider, Backlight, LV_ANIM_ON);
        bsp_set_backlight(Backlight);
    }
    else {
        printf("Backlight out of range: %d\n", Backlight);
    }
}

/**
 * @brief NTP sync button click event callback
 *
 * Triggers manual NTP time synchronization when clicked.
 * Only works when WiFi is connected.
 *
 * @param e LVGL event
 */
static void ntp_sync_btn_event_cb(lv_event_t *e)
{
    lv_obj_t * obj = (lv_obj_t*)lv_event_get_target(e);
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (wifi_manager_is_connected()) {
            printf("Manual NTP sync triggered\n");
            time_sync_now();
        } else {
            printf("Cannot sync: WiFi not connected\n");
        }
    }
}

/**
 * @brief Battery status update timer callback
 *
 * Called every 1 second to refresh battery information from AXP2101 PMU.
 * Updates all voltage and status labels in the battery message box.
 *
 * @param timer LVGL timer handle
 */
static void axp2101_time_cb(lv_timer_t *timer)
{
    /* Update status flags */
    lv_label_set_text(label_charging, PMU.isCharging() ? "YES" : "NO");
    lv_label_set_text(label_battery_connect, PMU.isBatteryConnect() ? "YES" : "NO");
    lv_label_set_text(label_vbus_in, PMU.isVbusIn() ? "YES" : "NO");

    /* Update battery info */
    lv_label_set_text_fmt(label_battery_percent, "%d %%", PMU.getBatteryPercent());
    lv_label_set_text_fmt(label_battery_voltage, "%d mV", PMU.getBattVoltage());
    lv_label_set_text_fmt(label_vbus_voltage, "%d mV", PMU.getVbusVoltage());

    /* Update power rail voltages */
    lv_label_set_text_fmt(label_system_voltage, "%d mV", PMU.getSystemVoltage());
    lv_label_set_text_fmt(label_dc1_voltage, "%d mV", PMU.getDC1Voltage());
    lv_label_set_text_fmt(label_aldo1_voltage, "%d mV", PMU.getALDO1Voltage());
    lv_label_set_text_fmt(label_bldo1_voltage, "%d mV", PMU.getBLDO1Voltage());
    lv_label_set_text_fmt(label_bldo2_voltage, "%d mV", PMU.getBLDO2Voltage());
}

/**
 * @brief Initialize battery information tile/list
 *
 * Creates a scrollable list showing all AXP2101 PMU readings:
 * - Charging status, battery connection, VBUS input
 * - Battery percentage and voltage
 * - Various power rail voltages (VBUS, System, DC1, ALDO1, BLDO1/2)
 *
 * Also starts a 1-second timer to update readings.
 *
 * @param parent Parent LVGL object to add the list to
 */
void axp2101_tile_init(lv_obj_t *parent)
{
    /* Create scrollable list for battery info */
    list_bat_msg = lv_list_create(parent);
    lv_obj_set_size(list_bat_msg, lv_pct(95), lv_pct(90));

    lv_obj_t *list_item;

    /* Charging status */
    list_item = lv_list_add_btn(list_bat_msg, NULL, "isCharging");
    label_charging = lv_label_create(list_item);
    lv_label_set_text(label_charging, PMU.isCharging() ? "YES" : "NO");

    /* Battery connection status */
    list_item = lv_list_add_btn(list_bat_msg, NULL, "isBatteryConnect");
    label_battery_connect = lv_label_create(list_item);
    lv_label_set_text(label_battery_connect, PMU.isBatteryConnect() ? "YES" : "NO");

    /* USB VBUS input status */
    list_item = lv_list_add_btn(list_bat_msg, NULL, "isVbusIn");
    label_vbus_in = lv_label_create(list_item);
    lv_label_set_text(label_vbus_in, PMU.isVbusIn() ? "YES" : "NO");

    /* Battery percentage */
    list_item = lv_list_add_btn(list_bat_msg, NULL, "BatteryPercent");
    label_battery_percent = lv_label_create(list_item);
    lv_label_set_text_fmt(label_battery_percent, "%d %%", PMU.getBatteryPercent());

    /* Battery voltage */
    list_item = lv_list_add_btn(list_bat_msg, NULL, "BatteryVoltage");
    label_battery_voltage = lv_label_create(list_item);
    lv_label_set_text_fmt(label_battery_voltage, "%d mV", PMU.getBattVoltage());

    /* VBUS voltage */
    list_item = lv_list_add_btn(list_bat_msg, NULL, "VbusVoltage");
    label_vbus_voltage = lv_label_create(list_item);
    lv_label_set_text_fmt(label_vbus_voltage, "%d mV", PMU.getVbusVoltage());

    /* System voltage */
    list_item = lv_list_add_btn(list_bat_msg, NULL, "SystemVoltage");
    label_system_voltage = lv_label_create(list_item);
    lv_label_set_text_fmt(label_system_voltage, "%d mV", PMU.getSystemVoltage());

    /* DC1 power rail voltage */
    list_item = lv_list_add_btn(list_bat_msg, NULL, "DC1Voltage");
    label_dc1_voltage = lv_label_create(list_item);
    lv_label_set_text_fmt(label_dc1_voltage, "%d mV", PMU.getDC1Voltage());

    /* ALDO1 power rail voltage */
    list_item = lv_list_add_btn(list_bat_msg, NULL, "ALDO1Voltage");
    label_aldo1_voltage = lv_label_create(list_item);
    lv_label_set_text_fmt(label_aldo1_voltage, "%d mV", PMU.getALDO1Voltage());

    /* BLDO1 power rail voltage */
    list_item = lv_list_add_btn(list_bat_msg, NULL, "BLDO1Voltage");
    label_bldo1_voltage = lv_label_create(list_item);
    lv_label_set_text_fmt(label_bldo1_voltage, "%d mV", PMU.getBLDO1Voltage());

    /* BLDO2 power rail voltage */
    list_item = lv_list_add_btn(list_bat_msg, NULL, "BLDO2Voltage");
    label_bldo2_voltage = lv_label_create(list_item);
    lv_label_set_text_fmt(label_bldo2_voltage, "%d mV", PMU.getBLDO2Voltage());

    /* Start timer to update values every second */
    axp_timer = lv_timer_create(axp2101_time_cb, 1000, NULL);
}

/**
 * @brief Battery message box close event callback
 *
 * Called when battery info message box is closed.
 * Stops the update timer to prevent accessing deleted objects.
 *
 * @param e LVGL event
 */
static void msg_box_button_exit_event_cb(lv_event_t * e)
{
    lv_obj_t * mbox = (lv_obj_t *) lv_event_get_user_data(e);

    /* Stop the update timer when closing */
    if (axp_timer != NULL) {
        lv_timer_del(axp_timer);
        axp_timer = NULL;
    }
}

/**
 * @brief Create and show battery information message box
 *
 * Creates a popup showing detailed AXP2101 PMU information
 * with live updating values.
 */
static void lv_create_msgbox(void)
{
    lv_obj_t * setting = lv_msgbox_create(NULL);
    lv_obj_set_style_clip_corner(setting, true, 0);

    lv_msgbox_add_title(setting, "AXP2101");

    /* Set fixed size for the message box */
    lv_obj_set_size(setting, 200, 250);

    /* Add close button and register cleanup callback */
    lv_obj_t * exit_but = lv_msgbox_add_close_button(setting);
    lv_obj_add_event_cb(setting, msg_box_button_exit_event_cb, LV_EVENT_DELETE, NULL);

    /* Configure content area with vertical flex layout */
    lv_obj_t * content = lv_msgbox_get_content(setting);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_right(content, -1, LV_PART_SCROLLBAR);

    /* Initialize battery info list inside content */
    axp2101_tile_init(content);
}

/**
 * @brief Battery button click event callback
 *
 * Opens the battery information popup when clicked.
 *
 * @param e LVGL event
 */
static void bat_btn_event_cb(lv_event_t *e)
{
    lv_obj_t * obj = (lv_obj_t*)lv_event_get_target(e);
    if (lv_obj_has_state(obj, LV_EVENT_CLICKED)) {
        lv_create_msgbox();
    }
}

/**
 * @brief WiFi message box close event callback
 *
 * Called when WiFi scan message box is closed.
 * Stops the WiFi scan task.
 *
 * @param e LVGL event
 */
static void wifi_msg_box_exit_event_cb(lv_event_t * e)
{
    lv_obj_t * mbox = (lv_obj_t *) lv_event_get_user_data(e);

    /* Stop WiFi scanning when closing */
    delete_lv_wifi_scan_task();
}

/**
 * @brief Create and show WiFi scan message box
 *
 * Creates a full-screen popup showing available WiFi networks.
 */
static void lv_create_wifi_msgbox(void)
{
    lv_obj_t * setting = lv_msgbox_create(NULL);
    lv_obj_set_style_clip_corner(setting, true, 0);

    lv_msgbox_add_title(setting, "wifi");

    /* Full-screen size for WiFi list */
    lv_obj_set_size(setting, 240, 284);

    /* Add close button and register cleanup callback */
    lv_obj_t * exit_but = lv_msgbox_add_close_button(setting);
    lv_obj_add_event_cb(setting, wifi_msg_box_exit_event_cb, LV_EVENT_DELETE, NULL);

    /* Configure content area */
    lv_obj_t * content = lv_msgbox_get_content(setting);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_right(content, -1, LV_PART_SCROLLBAR);

    /* Initialize WiFi scan UI */
    wifi_tile_init(content);
}

/**
 * @brief WiFi button click event callback
 *
 * Opens the WiFi scan popup when clicked.
 *
 * @param e LVGL event
 */
static void wifi_btn_event_cb(lv_event_t *e)
{
    lv_obj_t * obj = (lv_obj_t*)lv_event_get_target(e);
    if (lv_obj_has_state(obj, LV_EVENT_CLICKED)) {
        lv_create_wifi_msgbox();
    }
}

/**
 * @brief Logging switch toggle event callback
 *
 * Enables or disables file logging when switch is toggled.
 *
 * @param e LVGL event
 */
static void logging_switch_event_cb(lv_event_t *e)
{
    lv_obj_t *obj = (lv_obj_t*)lv_event_get_target(e);
    bool enabled = lv_obj_has_state(obj, LV_STATE_CHECKED);
    sd_logger_set_enabled(enabled);
    printf("File logging %s\n", enabled ? "enabled" : "disabled");
}

/**
 * @brief Clear logs button click event callback
 *
 * Clears all log files from SD card.
 *
 * @param e LVGL event
 */
static void clear_logs_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        sd_logger_clear_all();
        /* Update size display */
        if (logging_size_label != NULL) {
            lv_label_set_text(logging_size_label, "0 KB");
        }
        printf("Log files cleared\n");
    }
}

/**
 * @brief Update screen off timeout value display
 */
static void update_screen_off_value_label(uint32_t seconds)
{
    if (screen_off_value != NULL) {
        char buf[16];
        if (seconds >= 60) {
            snprintf(buf, sizeof(buf), "%lum", seconds / 60);
        } else {
            snprintf(buf, sizeof(buf), "%lus", seconds);
        }
        lv_label_set_text(screen_off_value, buf);
    }
}

/**
 * @brief Update sleep timeout value display
 */
static void update_sleep_value_label(uint32_t seconds)
{
    if (sleep_value != NULL) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%lum", seconds / 60);
        lv_label_set_text(sleep_value, buf);
    }
}

/**
 * @brief Screen off timeout slider change callback
 *
 * @param e LVGL event
 */
static void screen_off_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
    uint32_t seconds = (uint32_t)lv_slider_get_value(slider);
    power_manager_set_screen_off_timeout(seconds);
    update_screen_off_value_label(seconds);
}

/**
 * @brief Sleep timeout slider change callback
 *
 * @param e LVGL event
 */
static void sleep_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
    uint32_t seconds = (uint32_t)lv_slider_get_value(slider);
    power_manager_set_sleep_timeout(seconds);
    update_sleep_value_label(seconds);
}

/*===========================================================================
 * Motion Settings Callbacks
 *===========================================================================*/

static void update_motion_moving_value(float g)
{
    if (motion_moving_value != NULL) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2fg", g);
        lv_label_set_text(motion_moving_value, buf);
    }
}

static void update_motion_shaking_value(float g)
{
    if (motion_shaking_value != NULL) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1fg", g);
        lv_label_set_text(motion_shaking_value, buf);
    }
}

static void update_motion_rotating_value(float dps)
{
    if (motion_rotating_value != NULL) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f", dps);
        lv_label_set_text(motion_rotating_value, buf);
    }
}

static void update_motion_spinning_value(float dps)
{
    if (motion_spinning_value != NULL) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f", dps);
        lv_label_set_text(motion_spinning_value, buf);
    }
}

static void update_motion_braking_value(float gps)
{
    if (motion_braking_value != NULL) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", gps);
        lv_label_set_text(motion_braking_value, buf);
    }
}

static void motion_moving_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
    int value = lv_slider_get_value(slider);
    float g = (float)value / 100.0f;  /* Slider 10-100 -> 0.10-1.00g */
    motion_config_set_moving_threshold(g);
    update_motion_moving_value(g);
}

static void motion_shaking_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
    int value = lv_slider_get_value(slider);
    float g = (float)value / 10.0f;  /* Slider 10-50 -> 1.0-5.0g */
    motion_config_set_shaking_threshold(g);
    update_motion_shaking_value(g);
}

static void motion_rotating_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
    float dps = (float)lv_slider_get_value(slider);  /* Slider 10-100 deg/s */
    motion_config_set_rotating_threshold(dps);
    update_motion_rotating_value(dps);
}

static void motion_spinning_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
    float dps = (float)lv_slider_get_value(slider);  /* Slider 50-300 deg/s */
    motion_config_set_spinning_threshold(dps);
    update_motion_spinning_value(dps);
}

static void motion_braking_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
    int value = lv_slider_get_value(slider);
    float gps = (float)value / 10.0f;  /* Slider 10-100 -> 1.0-10.0 g/s */
    motion_config_set_braking_threshold(gps);
    update_motion_braking_value(gps);
}

static void motion_reset_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        motion_config_reset_defaults();

        /* Update all sliders and labels to default values */
        if (motion_moving_slider) {
            lv_slider_set_value(motion_moving_slider, (int)(MOTION_DEFAULT_MOVING_G * 100), LV_ANIM_ON);
            update_motion_moving_value(MOTION_DEFAULT_MOVING_G);
        }
        if (motion_shaking_slider) {
            lv_slider_set_value(motion_shaking_slider, (int)(MOTION_DEFAULT_SHAKING_G * 10), LV_ANIM_ON);
            update_motion_shaking_value(MOTION_DEFAULT_SHAKING_G);
        }
        if (motion_rotating_slider) {
            lv_slider_set_value(motion_rotating_slider, (int)MOTION_DEFAULT_ROTATING_DPS, LV_ANIM_ON);
            update_motion_rotating_value(MOTION_DEFAULT_ROTATING_DPS);
        }
        if (motion_spinning_slider) {
            lv_slider_set_value(motion_spinning_slider, (int)MOTION_DEFAULT_SPINNING_DPS, LV_ANIM_ON);
            update_motion_spinning_value(MOTION_DEFAULT_SPINNING_DPS);
        }
        if (motion_braking_slider) {
            lv_slider_set_value(motion_braking_slider, (int)(MOTION_DEFAULT_BRAKING_GPS * 10), LV_ANIM_ON);
            update_motion_braking_value(MOTION_DEFAULT_BRAKING_GPS);
        }
        printf("Motion thresholds reset to defaults\n");
    }
}

/**
 * @brief Update log size display label
 */
static void update_log_size_display(void)
{
    if (logging_size_label != NULL) {
        uint32_t size_kb = sd_logger_get_total_size_kb();
        char buf[32];
        if (size_kb >= 1024) {
            snprintf(buf, sizeof(buf), "%.1f MB", (float)size_kb / 1024.0f);
        } else {
            snprintf(buf, sizeof(buf), "%lu KB", size_kb);
        }
        lv_label_set_text(logging_size_label, buf);
    }
}

/**
 * @brief Get flash memory size
 *
 * Queries the ESP32 flash chip for its physical size.
 *
 * @return Flash size in MB, or 0 on error
 */
uint32_t Flash_Searching(void)
{
    uint32_t Flash_Size;
    if (esp_flash_get_physical_size(NULL, &Flash_Size) == ESP_OK) {
        Flash_Size = Flash_Size / (uint32_t)(1024 * 1024);
        printf("Flash size: %ld MB\n", Flash_Size);
        return Flash_Size;
    }
    else {
        printf("Get flash size failed\n");
        return 0;
    }
}

extern uint32_t SDCard_Size;

/*===========================================================================
 * App Lifecycle Methods
 *===========================================================================*/

/**
 * @brief Called when the settings app is launched
 *
 * Creates the main settings UI with:
 * - SD card size display
 * - Flash size display
 * - RTC time display (updated every second)
 * - Battery info button (opens popup)
 * - WiFi scan button (opens popup)
 * - Backlight brightness slider
 *
 * Uses LVGL grid layout for alignment.
 *
 * @return true on success
 */
bool PhoneSettingConf::run(void)
{
    ESP_BROOKESIA_LOGD("Run");

    /* Initialize muted text style (90% opacity) */
    lv_style_init(&style_text_muted);
    lv_style_set_text_opa(&style_text_muted, LV_OPA_90);

    /* Initialize dark placeholder text style for textareas */
    lv_style_init(&style_textarea_placeholder);
    lv_style_set_text_color(&style_textarea_placeholder, lv_color_hex(0x333333));

    lv_obj_t * panel1 = lv_obj_create(lv_screen_active());
    lv_obj_set_height(panel1, LV_SIZE_CONTENT);
    lv_obj_t * panel1_title = lv_label_create(panel1);
    lv_label_set_text(panel1_title, "Onboard parameter");

    lv_obj_t * SD_label = lv_label_create(panel1);
    lv_label_set_text(SD_label, "SD Card");
    SD_Size = lv_textarea_create(panel1);
    lv_textarea_set_one_line(SD_Size, true);
    lv_textarea_set_placeholder_text(SD_Size, "SD Size");
    lv_obj_add_style(SD_Size, &style_textarea_placeholder, LV_PART_TEXTAREA_PLACEHOLDER);
    uint32_t sd_size = get_sdcard_total_size();
    if(sd_size == 0)
    {
        lv_textarea_set_placeholder_text(SD_Size, "No SD card is mounted");
    }
    else
    {
        char buf[100]; 
        snprintf(buf, sizeof(buf), "%ld MB\r\n", sd_size);
        lv_textarea_set_placeholder_text(SD_Size, buf);
    }
    
    //FLASH SIZE
    lv_obj_t * Flash_label = lv_label_create(panel1);
    lv_label_set_text(Flash_label, "Flash Size");
    FlashSize = lv_textarea_create(panel1);
    lv_textarea_set_one_line(FlashSize, true);
    lv_textarea_set_placeholder_text(FlashSize, "Flash Size");
    lv_obj_add_style(FlashSize, &style_textarea_placeholder, LV_PART_TEXTAREA_PLACEHOLDER);
    uint32_t flash_size = Flash_Searching();
    if(flash_size == 0)
    {
        lv_textarea_set_placeholder_text(FlashSize, "get flash size err");
    }
    else
    {
        char buf[100]; 
        snprintf(buf, sizeof(buf), "%ld MB\r\n", flash_size);
        lv_textarea_set_placeholder_text(FlashSize, buf);
    }


    lv_obj_t * Time_label = lv_label_create(panel1);
    lv_label_set_text(Time_label, "RTC Time");
    lv_obj_add_style(Time_label, &style_text_muted, 0);

    RTC_Time = lv_textarea_create(panel1);
    lv_textarea_set_one_line(RTC_Time, true);
    lv_textarea_set_placeholder_text(RTC_Time, "Display time");
    lv_obj_add_style(RTC_Time, &style_textarea_placeholder, LV_PART_TEXTAREA_PLACEHOLDER);

    /* NTP Sync button - enabled only when WiFi is connected */
    but_ntp_sync = lv_button_create(panel1);
    lv_obj_set_size(but_ntp_sync, 60, 35);
    lv_obj_add_event_cb(but_ntp_sync, ntp_sync_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * ntp_sync_label = lv_label_create(but_ntp_sync);
    lv_obj_align(ntp_sync_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(ntp_sync_label, "Sync");
    /* Set initial state based on WiFi connection */
    if (!wifi_manager_is_connected()) {
        lv_obj_add_state(but_ntp_sync, LV_STATE_DISABLED);
    }

    /* Last NTP Sync time display */
    lv_obj_t * Last_NTP_label = lv_label_create(panel1);
    lv_label_set_text(Last_NTP_label, "Last NTP Sync");
    lv_obj_add_style(Last_NTP_label, &style_text_muted, 0);

    Last_NTP_Sync = lv_textarea_create(panel1);
    lv_textarea_set_one_line(Last_NTP_Sync, true);
    lv_obj_add_style(Last_NTP_Sync, &style_textarea_placeholder, LV_PART_TEXTAREA_PLACEHOLDER);
    char ntp_buf[32];
    time_sync_get_last_ntp_str(ntp_buf, sizeof(ntp_buf));
    lv_textarea_set_placeholder_text(Last_NTP_Sync, ntp_buf);

    lv_obj_t * bat_label = lv_label_create(panel1);
    lv_label_set_text(bat_label, "battery");
    lv_obj_add_style(bat_label, &style_text_muted, 0);
    but_bat_msg = lv_button_create(panel1);
    lv_obj_set_size(but_bat_msg,200,35);
    lv_obj_add_event_cb(but_bat_msg, bat_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * bat_msg_label = lv_label_create(but_bat_msg);
    lv_obj_align(bat_msg_label,LV_ALIGN_CENTER,0,0);
    lv_label_set_text(bat_msg_label, "Battery Info");
    lv_obj_add_style(bat_msg_label, &style_text_muted, 0);

    lv_obj_t * wifi_label = lv_label_create(panel1);
    lv_label_set_text(wifi_label, "wifi");
    lv_obj_add_style(wifi_label, &style_text_muted, 0);
    but_wifi_msg = lv_button_create(panel1);
    lv_obj_set_size(but_wifi_msg,200,35);
    lv_obj_add_event_cb(but_wifi_msg, wifi_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * wifi_msg_label = lv_label_create(but_wifi_msg);
    lv_obj_align(wifi_msg_label,LV_ALIGN_CENTER,0,0);
    lv_label_set_text(wifi_msg_label, "wifi Info`");
    lv_obj_add_style(wifi_msg_label, &style_text_muted, 0);


    lv_obj_t * Backlight_label = lv_label_create(panel1);
    lv_label_set_text(Backlight_label, "Backlight brightness");
    Backlight_slider = lv_slider_create(panel1);                                 
    lv_obj_add_flag(Backlight_slider, LV_OBJ_FLAG_CLICKABLE);    
    lv_obj_set_size(Backlight_slider, 100, 40);              
    lv_obj_set_style_radius(Backlight_slider, 3, LV_PART_KNOB);               // Adjust the value for more or less rounding                                            
    lv_obj_set_style_bg_opa(Backlight_slider, LV_OPA_TRANSP, LV_PART_KNOB);                               
    // lv_obj_set_style_pad_all(Backlight_slider, 0, LV_PART_KNOB);                                            
    lv_obj_set_style_bg_color(Backlight_slider, lv_color_hex(0xAAAAAA), LV_PART_KNOB);               
    lv_obj_set_style_bg_color(Backlight_slider, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);             
    lv_obj_set_style_outline_width(Backlight_slider, 2, LV_PART_INDICATOR);  
    lv_obj_set_style_outline_color(Backlight_slider, lv_color_hex(0xD3D3D3), LV_PART_INDICATOR);      
    lv_slider_set_range(Backlight_slider, 5, Backlight_MAX);
    lv_slider_set_value(Backlight_slider, DEFAULT_BACKLIGHT, LV_ANIM_ON);
    lv_obj_add_event_cb(Backlight_slider, Backlight_adjustment_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* File Logging section */
    lv_obj_t * logging_label = lv_label_create(panel1);
    lv_label_set_text(logging_label, "File Logging");
    lv_obj_add_style(logging_label, &style_text_muted, 0);

    /* Logging enable switch */
    logging_switch = lv_switch_create(panel1);
    lv_obj_set_size(logging_switch, 50, 25);
    if (sd_logger_is_enabled()) {
        lv_obj_add_state(logging_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(logging_switch, logging_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Log size label */
    logging_size_label = lv_label_create(panel1);
    update_log_size_display();

    /* Clear logs button */
    but_clear_logs = lv_button_create(panel1);
    lv_obj_set_size(but_clear_logs, 60, 25);
    lv_obj_add_event_cb(but_clear_logs, clear_logs_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * clear_logs_label = lv_label_create(but_clear_logs);
    lv_obj_align(clear_logs_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(clear_logs_label, "Clear");

    /* Sleep Settings section */
    lv_obj_t * sleep_label = lv_label_create(panel1);
    lv_label_set_text(sleep_label, "Sleep Settings (Face-down)");
    lv_obj_add_style(sleep_label, &style_text_muted, 0);

    /* Screen off timeout slider */
    lv_obj_t * screen_off_label = lv_label_create(panel1);
    lv_label_set_text(screen_off_label, "Screen off:");
    screen_off_slider = lv_slider_create(panel1);
    lv_obj_set_size(screen_off_slider, 100, 25);
    lv_slider_set_range(screen_off_slider, 10, 600);
    lv_slider_set_value(screen_off_slider, power_manager_get_screen_off_timeout(), LV_ANIM_OFF);
    lv_obj_add_event_cb(screen_off_slider, screen_off_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    screen_off_value = lv_label_create(panel1);
    update_screen_off_value_label(power_manager_get_screen_off_timeout());

    /* Light sleep timeout slider */
    lv_obj_t * sleep_timeout_label = lv_label_create(panel1);
    lv_label_set_text(sleep_timeout_label, "Sleep:");
    sleep_slider = lv_slider_create(panel1);
    lv_obj_set_size(sleep_slider, 100, 25);
    lv_slider_set_range(sleep_slider, 60, 1800);
    lv_slider_set_value(sleep_slider, power_manager_get_sleep_timeout(), LV_ANIM_OFF);
    lv_obj_add_event_cb(sleep_slider, sleep_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    sleep_value = lv_label_create(panel1);
    update_sleep_value_label(power_manager_get_sleep_timeout());

    /* Motion Settings section */
    lv_obj_t * motion_label = lv_label_create(panel1);
    lv_label_set_text(motion_label, "Motion Thresholds");
    lv_obj_add_style(motion_label, &style_text_muted, 0);

    /* Moving threshold slider (0.10 - 1.00 g) */
    lv_obj_t * moving_label = lv_label_create(panel1);
    lv_label_set_text(moving_label, "Moving:");
    motion_moving_slider = lv_slider_create(panel1);
    lv_obj_set_size(motion_moving_slider, 80, 25);
    lv_slider_set_range(motion_moving_slider, 10, 100);  /* 0.10 - 1.00 g */
    lv_slider_set_value(motion_moving_slider, (int)(motion_config_get_moving_threshold() * 100), LV_ANIM_OFF);
    lv_obj_add_event_cb(motion_moving_slider, motion_moving_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    motion_moving_value = lv_label_create(panel1);
    update_motion_moving_value(motion_config_get_moving_threshold());

    /* Shaking threshold slider (1.0 - 5.0 g) */
    lv_obj_t * shaking_label = lv_label_create(panel1);
    lv_label_set_text(shaking_label, "Shaking:");
    motion_shaking_slider = lv_slider_create(panel1);
    lv_obj_set_size(motion_shaking_slider, 80, 25);
    lv_slider_set_range(motion_shaking_slider, 10, 50);  /* 1.0 - 5.0 g */
    lv_slider_set_value(motion_shaking_slider, (int)(motion_config_get_shaking_threshold() * 10), LV_ANIM_OFF);
    lv_obj_add_event_cb(motion_shaking_slider, motion_shaking_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    motion_shaking_value = lv_label_create(panel1);
    update_motion_shaking_value(motion_config_get_shaking_threshold());

    /* Rotating threshold slider (10 - 100 deg/s) */
    lv_obj_t * rotating_label = lv_label_create(panel1);
    lv_label_set_text(rotating_label, "Rotating:");
    motion_rotating_slider = lv_slider_create(panel1);
    lv_obj_set_size(motion_rotating_slider, 80, 25);
    lv_slider_set_range(motion_rotating_slider, 10, 100);
    lv_slider_set_value(motion_rotating_slider, (int)motion_config_get_rotating_threshold(), LV_ANIM_OFF);
    lv_obj_add_event_cb(motion_rotating_slider, motion_rotating_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    motion_rotating_value = lv_label_create(panel1);
    update_motion_rotating_value(motion_config_get_rotating_threshold());

    /* Spinning threshold slider (50 - 300 deg/s) */
    lv_obj_t * spinning_label = lv_label_create(panel1);
    lv_label_set_text(spinning_label, "Spinning:");
    motion_spinning_slider = lv_slider_create(panel1);
    lv_obj_set_size(motion_spinning_slider, 80, 25);
    lv_slider_set_range(motion_spinning_slider, 50, 300);
    lv_slider_set_value(motion_spinning_slider, (int)motion_config_get_spinning_threshold(), LV_ANIM_OFF);
    lv_obj_add_event_cb(motion_spinning_slider, motion_spinning_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    motion_spinning_value = lv_label_create(panel1);
    update_motion_spinning_value(motion_config_get_spinning_threshold());

    /* Braking threshold slider (1.0 - 10.0 g/s) */
    lv_obj_t * braking_label = lv_label_create(panel1);
    lv_label_set_text(braking_label, "Braking:");
    motion_braking_slider = lv_slider_create(panel1);
    lv_obj_set_size(motion_braking_slider, 80, 25);
    lv_slider_set_range(motion_braking_slider, 10, 100);  /* 1.0 - 10.0 g/s */
    lv_slider_set_value(motion_braking_slider, (int)(motion_config_get_braking_threshold() * 10), LV_ANIM_OFF);
    lv_obj_add_event_cb(motion_braking_slider, motion_braking_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    motion_braking_value = lv_label_create(panel1);
    update_motion_braking_value(motion_config_get_braking_threshold());

    /* Reset defaults button */
    motion_reset_btn = lv_button_create(panel1);
    lv_obj_set_size(motion_reset_btn, 60, 25);
    lv_obj_add_event_cb(motion_reset_btn, motion_reset_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * reset_label = lv_label_create(motion_reset_btn);
    lv_obj_align(reset_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(reset_label, "Reset");

    static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t grid_main_row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(lv_screen_active(), grid_main_col_dsc, grid_main_row_dsc);

    static lv_coord_t grid_2_col_dsc[] = {LV_GRID_FR(5), LV_GRID_FR(5), LV_GRID_FR(50), LV_GRID_FR(5), LV_GRID_FR(5), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t grid_2_row_dsc[] = {
        LV_GRID_CONTENT,  /*0: Title*/
        5,                /*1: Separator*/
        LV_GRID_CONTENT,  /*2: SD label*/
        40,               /*3: SD Size*/
        LV_GRID_CONTENT,  /*4: Flash label*/
        40,               /*5: Flash Size*/
        LV_GRID_CONTENT,  /*6: Time label*/
        40,               /*7: RTC Time*/
        LV_GRID_CONTENT,  /*8: Last NTP label*/
        40,               /*9: Last NTP Sync*/
        LV_GRID_CONTENT,  /*10: bat label*/
        40,               /*11: bat button*/
        LV_GRID_CONTENT,  /*12: wifi label*/
        40,               /*13: wifi button*/
        LV_GRID_CONTENT,  /*14: Backlight label*/
        40,               /*15: Backlight slider*/
        LV_GRID_CONTENT,  /*16: Logging label*/
        35,               /*17: Logging controls*/
        LV_GRID_CONTENT,  /*18: Sleep Settings label*/
        35,               /*19: Screen off slider row*/
        35,               /*20: Sleep slider row*/
        LV_GRID_CONTENT,  /*21: Motion Settings label*/
        LV_GRID_CONTENT,  /*22: Moving label*/
        30,               /*23: Moving slider row*/
        LV_GRID_CONTENT,  /*24: Shaking label*/
        30,               /*25: Shaking slider row*/
        LV_GRID_CONTENT,  /*26: Rotating label*/
        30,               /*27: Rotating slider row*/
        LV_GRID_CONTENT,  /*28: Spinning label*/
        30,               /*29: Spinning slider row*/
        LV_GRID_CONTENT,  /*30: Braking label*/
        30,               /*31: Braking slider row*/
        35,               /*32: Reset button row*/
        30,               /*33: Bottom padding/gap*/
        LV_GRID_TEMPLATE_LAST
    };

    // 应用网格描述（确保只设置一次，覆盖之前的 grid_1 配置）
    lv_obj_set_grid_dsc_array(panel1, grid_2_col_dsc, grid_2_row_dsc);
    lv_obj_set_grid_cell(panel1, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1); // 主面板位置

    // 标题（占满列，行0）
    lv_obj_set_grid_cell(panel1_title, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_CENTER, 0, 1);

    // SD卡模块（行2-3）
    lv_obj_set_grid_cell(SD_label, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_START, 2, 1);
    lv_obj_set_grid_cell(SD_Size, LV_GRID_ALIGN_STRETCH, 0, 5, LV_GRID_ALIGN_CENTER, 3, 1);

    // Flash模块（行4-5）
    lv_obj_set_grid_cell(Flash_label, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_START, 4, 1);
    lv_obj_set_grid_cell(FlashSize, LV_GRID_ALIGN_STRETCH, 0, 5, LV_GRID_ALIGN_CENTER, 5, 1);

    // 时间模块（行6-7）- RTC time spans 4 columns, Sync button in last column
    lv_obj_set_grid_cell(Time_label, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_START, 6, 1);
    lv_obj_set_grid_cell(RTC_Time, LV_GRID_ALIGN_STRETCH, 0, 4, LV_GRID_ALIGN_CENTER, 7, 1);
    lv_obj_set_grid_cell(but_ntp_sync, LV_GRID_ALIGN_CENTER, 4, 1, LV_GRID_ALIGN_CENTER, 7, 1);

    // Last NTP Sync module (rows 8-9)
    lv_obj_set_grid_cell(Last_NTP_label, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_START, 8, 1);
    lv_obj_set_grid_cell(Last_NTP_Sync, LV_GRID_ALIGN_STRETCH, 0, 5, LV_GRID_ALIGN_CENTER, 9, 1);

    // 电池模块（行10-11）
    lv_obj_set_grid_cell(bat_label, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_START, 10, 1);
    lv_obj_set_grid_cell(but_bat_msg, LV_GRID_ALIGN_STRETCH, 0, 5, LV_GRID_ALIGN_CENTER, 11, 1);

    // WiFi模块（行12-13）
    lv_obj_set_grid_cell(wifi_label, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_START, 12, 1);
    lv_obj_set_grid_cell(but_wifi_msg, LV_GRID_ALIGN_STRETCH, 0, 5, LV_GRID_ALIGN_CENTER, 13, 1);

    // 背光模块（行14-15）
    lv_obj_set_grid_cell(Backlight_label, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_START, 14, 1);
    lv_obj_set_grid_cell(Backlight_slider, LV_GRID_ALIGN_STRETCH, 0, 5, LV_GRID_ALIGN_CENTER, 15, 1);

    // File Logging module (rows 16-17)
    lv_obj_set_grid_cell(logging_label, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_START, 16, 1);
    lv_obj_set_grid_cell(logging_switch, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 17, 1);
    lv_obj_set_grid_cell(logging_size_label, LV_GRID_ALIGN_CENTER, 1, 2, LV_GRID_ALIGN_CENTER, 17, 1);
    lv_obj_set_grid_cell(but_clear_logs, LV_GRID_ALIGN_END, 3, 2, LV_GRID_ALIGN_CENTER, 17, 1);

    // Sleep Settings module (rows 18-20)
    lv_obj_set_grid_cell(sleep_label, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_START, 18, 1);
    lv_obj_set_grid_cell(screen_off_label, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 19, 1);
    lv_obj_set_grid_cell(screen_off_slider, LV_GRID_ALIGN_STRETCH, 1, 3, LV_GRID_ALIGN_CENTER, 19, 1);
    lv_obj_set_grid_cell(screen_off_value, LV_GRID_ALIGN_END, 4, 1, LV_GRID_ALIGN_CENTER, 19, 1);
    lv_obj_set_grid_cell(sleep_timeout_label, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 20, 1);
    lv_obj_set_grid_cell(sleep_slider, LV_GRID_ALIGN_STRETCH, 1, 3, LV_GRID_ALIGN_CENTER, 20, 1);
    lv_obj_set_grid_cell(sleep_value, LV_GRID_ALIGN_END, 4, 1, LV_GRID_ALIGN_CENTER, 20, 1);

    // Motion Settings module (rows 21-32) - labels on separate rows above sliders
    lv_obj_set_grid_cell(motion_label, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_START, 21, 1);
    // Moving: label on row 22, slider+value on row 23
    lv_obj_set_grid_cell(moving_label, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_CENTER, 22, 1);
    lv_obj_set_grid_cell(motion_moving_slider, LV_GRID_ALIGN_STRETCH, 0, 4, LV_GRID_ALIGN_CENTER, 23, 1);
    lv_obj_set_grid_cell(motion_moving_value, LV_GRID_ALIGN_END, 4, 1, LV_GRID_ALIGN_CENTER, 23, 1);
    // Shaking: label on row 24, slider+value on row 25
    lv_obj_set_grid_cell(shaking_label, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_CENTER, 24, 1);
    lv_obj_set_grid_cell(motion_shaking_slider, LV_GRID_ALIGN_STRETCH, 0, 4, LV_GRID_ALIGN_CENTER, 25, 1);
    lv_obj_set_grid_cell(motion_shaking_value, LV_GRID_ALIGN_END, 4, 1, LV_GRID_ALIGN_CENTER, 25, 1);
    // Rotating: label on row 26, slider+value on row 27
    lv_obj_set_grid_cell(rotating_label, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_CENTER, 26, 1);
    lv_obj_set_grid_cell(motion_rotating_slider, LV_GRID_ALIGN_STRETCH, 0, 4, LV_GRID_ALIGN_CENTER, 27, 1);
    lv_obj_set_grid_cell(motion_rotating_value, LV_GRID_ALIGN_END, 4, 1, LV_GRID_ALIGN_CENTER, 27, 1);
    // Spinning: label on row 28, slider+value on row 29
    lv_obj_set_grid_cell(spinning_label, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_CENTER, 28, 1);
    lv_obj_set_grid_cell(motion_spinning_slider, LV_GRID_ALIGN_STRETCH, 0, 4, LV_GRID_ALIGN_CENTER, 29, 1);
    lv_obj_set_grid_cell(motion_spinning_value, LV_GRID_ALIGN_END, 4, 1, LV_GRID_ALIGN_CENTER, 29, 1);
    // Braking: label on row 30, slider+value on row 31
    lv_obj_set_grid_cell(braking_label, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_CENTER, 30, 1);
    lv_obj_set_grid_cell(motion_braking_slider, LV_GRID_ALIGN_STRETCH, 0, 4, LV_GRID_ALIGN_CENTER, 31, 1);
    lv_obj_set_grid_cell(motion_braking_value, LV_GRID_ALIGN_END, 4, 1, LV_GRID_ALIGN_CENTER, 31, 1);
    // Reset button on row 32
    lv_obj_set_grid_cell(motion_reset_btn, LV_GRID_ALIGN_CENTER, 0, 5, LV_GRID_ALIGN_CENTER, 32, 1);

    // Bottom separator line (row 33) - creates gap at bottom of settings
    lv_obj_t *bottom_line = lv_obj_create(panel1);
    lv_obj_set_size(bottom_line, LV_PCT(100), 2);
    lv_obj_set_style_bg_color(bottom_line, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(bottom_line, LV_OPA_50, 0);
    lv_obj_set_style_border_width(bottom_line, 0, 0);
    lv_obj_set_style_radius(bottom_line, 0, 0);
    lv_obj_set_grid_cell(bottom_line, LV_GRID_ALIGN_STRETCH, 0, 5, LV_GRID_ALIGN_CENTER, 33, 1);

    auto_step_timer = lv_timer_create(example1_increase_lvgl_tick, 1000, NULL);

    return true;
}

bool PhoneSettingConf::back(void)
{
    ESP_BROOKESIA_LOGD("Back");

    // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
    ESP_BROOKESIA_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");



    return true;
}



static void example1_increase_lvgl_tick(lv_timer_t * t)
{
    char buf[100];
    pcf85063a_datetime_t now_time;
    get_rtc_data_to_str(&now_time);
    snprintf(buf, sizeof(buf), "%d.%d.%d   %d:%d:%d\r\n",now_time.year,now_time.month,now_time.day,now_time.hour,now_time.min,now_time.sec);
    lv_textarea_set_placeholder_text(RTC_Time, buf);

    /* Update Last NTP Sync display */
    if (Last_NTP_Sync != NULL) {
        char ntp_buf[32];
        time_sync_get_last_ntp_str(ntp_buf, sizeof(ntp_buf));
        lv_textarea_set_placeholder_text(Last_NTP_Sync, ntp_buf);
    }

    /* Update NTP sync button state based on WiFi connection */
    if (but_ntp_sync != NULL) {
        if (wifi_manager_is_connected()) {
            lv_obj_clear_state(but_ntp_sync, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(but_ntp_sync, LV_STATE_DISABLED);
        }
    }

    /* Update log size display (every 5 seconds to reduce SD card access) */
    static int log_update_counter = 0;
    if (++log_update_counter >= 5) {
        log_update_counter = 0;
        update_log_size_display();
    }
}

bool PhoneSettingConf::close(void)
{
    ESP_BROOKESIA_LOGD("Close");
    printf(" close\r\n");
    /* Do some operations here if needed */
    ESP_BROOKESIA_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
    return true;
}

// bool PhoneAppComplexConf::init()
// {
//     ESP_BROOKESIA_LOGD("Init");

//     /* Do some initialization here if needed */

//     return true;
// }

// bool PhoneAppComplexConf::deinit()
// {
//     ESP_BROOKESIA_LOGD("Deinit");

//     /* Do some deinitialization here if needed */

//     return true;
// }

// bool PhoneAppComplexConf::pause()
// {
//     ESP_BROOKESIA_LOGD("Pause");

//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppComplexConf::resume()
// {
//     ESP_BROOKESIA_LOGD("Resume");

//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppComplexConf::cleanResource()
// {
//     ESP_BROOKESIA_LOGD("Clean resource");

//     /* Do some cleanup here if needed */

//     return true;
// }
