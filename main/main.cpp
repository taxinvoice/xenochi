/**
 * @file main.cpp
 * @brief Main application entry point for the Xenochi ESP32-C6 smart device
 *
 * This file initializes the hardware, sets up the ESP-Brookesia phone UI framework,
 * and installs all available applications (Music, Settings, Gyroscope, Recorder).
 *
 * Application Flow:
 * 1. Initialize board peripherals (LCD, touch, I2C, I2S, SD card, etc.)
 * 2. Initialize power management (AXP2101 PMU)
 * 3. Scan SD card for music files
 * 4. Create phone UI with dark theme stylesheet
 * 5. Install all apps and start clock update timer
 *
 * @note This project uses ESP-Brookesia v0.5.0 for the phone-like UI framework
 * @note LVGL v9.2.2 is used for all graphics rendering
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_board.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "240_284/dark/stylesheet.hpp"
#include "esp_brookesia.hpp"
#include "private/esp_brookesia_utils.h"
#include "lvgl_music.h"

/* Application headers - each app is a separate component */
#include "lvgl_app_gyroscope.hpp"   /* IMU sensor data display */
#include "lvgl_app_setting.hpp"     /* System settings and diagnostics */
#include "lvgl_app_music.hpp"       /* Music player with file browser */
#include "lvgl_app_rec.hpp"         /* Audio recorder to WAV */
#include "lvgl_app_mibuddy.hpp"     /* MiBuddy virtual buddy (image slideshow) */
#include "audio_driver.h"           /* Audio playback control */
#include "wifi_manager.h"           /* WiFi auto-connect and status */
#include "time_sync.h"              /* NTP time sync to RTC */
#include "sd_logger.h"              /* SD card file logging */
#include "power_manager.h"          /* Face-down sleep mode */

#include "esp_heap_caps.h"

static const char *TAG = "app_main";

/**
 * @brief Global phone pointer for WiFi status callback
 *
 * The WiFi manager callback needs access to the phone object to update
 * the status bar WiFi icon. This pointer is set after phone initialization.
 */
static ESP_Brookesia_Phone *g_phone = nullptr;

/**
 * @brief WiFi status callback - updates status bar icon based on connection state
 *
 * Called by wifi_manager when WiFi connection status changes.
 * Updates the status bar WiFi icon to reflect:
 * - Disconnected (state 0): No connection
 * - Signal level 1 (state 1): Weak signal (RSSI < -70 dBm)
 * - Signal level 2 (state 2): Medium signal (-70 to -50 dBm)
 * - Signal level 3 (state 3): Strong signal (RSSI > -50 dBm)
 *
 * @param connected true if connected to WiFi
 * @param rssi Signal strength in dBm (only valid when connected)
 *
 * @note Called from ESP event task context, must lock LVGL mutex
 */
static void on_wifi_status_changed(bool connected, int rssi)
{
    /* Determine icon state based on connection and signal strength */
    int icon_state = 0;  /* Disconnected */

    if (connected) {
        if (rssi > -50) {
            icon_state = 3;  /* Strong signal */
        } else if (rssi > -70) {
            icon_state = 2;  /* Medium signal */
        } else {
            icon_state = 1;  /* Weak signal */
        }
        ESP_LOGI(TAG, "WiFi connected, RSSI: %d dBm, icon state: %d", rssi, icon_state);
    } else {
        ESP_LOGW(TAG, "WiFi disconnected, icon state: 0");
    }

    /* Update status bar icon (thread-safe with LVGL lock) */
    if (g_phone != nullptr) {
        if (lvgl_port_lock(100)) {
            g_phone->getHome().getStatusBar()->setWifiIconState(icon_state);
            lvgl_port_unlock();
        } else {
            ESP_LOGW(TAG, "Failed to acquire LVGL lock for WiFi icon update");
        }
    }
}

/**
 * @brief Find battery icon images in the status bar and set their recolor
 *
 * Traverses the LVGL object tree to find the battery icon images
 * in the status bar and updates their recolor based on battery level.
 *
 * @param parent The parent object to search in
 * @param color The color to apply to the battery icon
 */
static void set_battery_icon_color_recursive(lv_obj_t *obj, lv_color_t color)
{
    if (obj == NULL) return;

    /* Check if this is an image object */
    if (lv_obj_check_type(obj, &lv_image_class)) {
        lv_obj_set_style_img_recolor(obj, color, 0);
        lv_obj_set_style_img_recolor_opa(obj, LV_OPA_COVER, 0);
    }

    /* Recurse into children */
    uint32_t child_count = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < child_count; i++) {
        set_battery_icon_color_recursive(lv_obj_get_child(obj, i), color);
    }
}

/**
 * @brief Find the battery icon container in the status bar
 *
 * The battery icon is in area 2 (right side) of the status bar.
 * Status bar structure: main_obj -> area_objs[0,1,2] -> icons
 * Battery icon is typically the first icon in area 2.
 *
 * @param status_bar_obj The status bar main object
 * @return The battery icon container, or NULL if not found
 */
static lv_obj_t *find_battery_icon_container(lv_obj_t *status_bar_obj)
{
    if (status_bar_obj == NULL) return NULL;

    /* Area 2 is the third child (index 2) of the status bar */
    uint32_t child_count = lv_obj_get_child_count(status_bar_obj);
    if (child_count < 3) return NULL;

    lv_obj_t *area_2 = lv_obj_get_child(status_bar_obj, 2);
    if (area_2 == NULL) return NULL;

    /* Battery icon container is the first child of area 2 (per status_bar_data.hpp order) */
    /* Actually, wifi is added first in beginWifi(), then battery in beginBattery() */
    /* So battery is child index 1 (second child) */
    child_count = lv_obj_get_child_count(area_2);
    if (child_count < 1) return NULL;

    /* Battery container is typically child 0 (first added to area 2) */
    return lv_obj_get_child(area_2, 0);
}

/**
 * @brief Timer callback to update battery status and icon color
 *
 * Updates the status bar battery icon:
 * - Gets battery percentage from AXP2101 PMU
 * - Updates the icon level via setBatteryPercent()
 * - Changes icon color: RED (<5%), ORANGE (<20%), WHITE (normal)
 *
 * @param t LVGL timer handle containing the phone object as user_data
 */
static void on_battery_update_timer_cb(struct _lv_timer_t *t)
{
    ESP_Brookesia_Phone *phone = (ESP_Brookesia_Phone *)t->user_data;
    if (phone == NULL) return;

    /* Get battery status */
    int battery_pct = bsp_battery_get_percent();
    bool is_charging = bsp_battery_is_charging();

    /* Use 100% if no battery connected (USB powered) */
    if (battery_pct < 0) battery_pct = 100;

    /* Update status bar battery icon level */
    ESP_Brookesia_StatusBar *status_bar = phone->getHome().getStatusBar();
    if (status_bar != NULL) {
        status_bar->setBatteryPercent(is_charging, battery_pct);
    }

    /* Determine battery color based on level */
    lv_color_t battery_color;
    if (!is_charging && battery_pct < 5) {
        battery_color = lv_color_hex(0xFF4444);  /* Red for critical */
    } else if (!is_charging && battery_pct < 20) {
        battery_color = lv_color_hex(0xFFA500);  /* Orange for low */
    } else {
        battery_color = lv_color_hex(0xFFFFFF);  /* White for normal */
    }

    /* Find and update battery icon color
     * The status bar is the first child at y=0 on the active screen
     */
    lv_obj_t *screen = lv_screen_active();
    if (screen == NULL) return;

    /* Status bar should be the first child of the screen */
    lv_obj_t *status_bar_obj = lv_obj_get_child(screen, 0);
    if (status_bar_obj == NULL) return;

    /* Find the battery icon container (in area 2) */
    lv_obj_t *battery_container = find_battery_icon_container(status_bar_obj);
    if (battery_container != NULL) {
        set_battery_icon_color_recursive(battery_container, battery_color);
    }
}

/**
 * @brief Timer callback to update the status bar clock display
 *
 * This callback is triggered every 1000ms to refresh the clock in the
 * phone's status bar. It reads the current system time and updates
 * the hour:minute display.
 *
 * @param t LVGL timer handle containing the phone object as user_data
 *
 * @note Called from LVGL task context, so LVGL operations are thread-safe
 */
static void on_clock_update_timer_cb(struct _lv_timer_t *t)
{
    time_t now;
    struct tm timeinfo;
    ESP_Brookesia_Phone *phone = (ESP_Brookesia_Phone *)t->user_data;

    /* Get current system time */
    time(&now);
    localtime_r(&now, &timeinfo);

    /* Update clock on status bar (safe to call from LVGL timer context) */
    ESP_BROOKESIA_CHECK_FALSE_EXIT(
        phone->getHome().getStatusBar()->setClock(timeinfo.tm_hour, timeinfo.tm_min),
        "Refresh status bar failed"
    );
}

/**
 * @brief Main application entry point
 *
 * Initializes all hardware peripherals, creates the phone UI framework,
 * installs applications, and starts the main event loop.
 *
 * Initialization sequence:
 * 1. BSP initialization (LCD, touch, I2C, I2S, SD card, RTC, IMU)
 * 2. Power management (AXP2101) initialization
 * 3. Music file scanning on SD card
 * 4. ESP-Brookesia phone UI creation with dark theme
 * 5. App installation (Music, Settings, Gyroscope, Recorder)
 * 6. Clock update timer creation
 */
extern "C" void app_main(void)
{
    /*==========================================================================
     * PHASE 1: Hardware Initialization
     *=========================================================================*/

    /* Initialize NVS flash (required for WiFi)
     * NVS stores WiFi calibration data and other persistent settings
     */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize all board peripherals:
     * - I2C bus (for touch, RTC, IMU, PMU)
     * - SPI bus (for LCD, SD card)
     * - I2S (for audio)
     * - LCD display driver
     * - Touch driver (CST816S)
     * - SD card filesystem
     * - RTC (PCF85063A)
     * - IMU (QMI8658)
     */
    bsp_init();

    /* Get handles to initialized peripherals for later use */
    bsp_handles_t *handles = bsp_display_get_handles();

    /* Initialize AXP2101 Power Management Unit
     * - Configures power rails for LCD, audio, etc.
     * - Enables battery monitoring and charging
     */
    ESP_ERROR_CHECK(AXP2101_driver_init());

    /*==========================================================================
     * PHASE 1.5: WiFi Initialization
     *=========================================================================*/

    /* Initialize WiFi manager with status callback
     * - Connects to network configured in menuconfig (MiBuddy WiFi Configuration)
     * - Status callback updates the status bar WiFi icon
     * - Auto-retries forever on disconnect
     *
     * Note: WiFi starts connecting in the background while UI loads
     */
    wifi_manager_init(on_wifi_status_changed);

    /* Initialize time sync module
     * - Syncs time from NTP server when WiFi connects
     * - Updates hardware RTC (PCF85063A) with synchronized time
     * - Timezone configured in menuconfig (MiBuddy Time Sync Configuration)
     */
    time_sync_init(NULL);

    /* Initialize SD card file logger
     * - Hooks into ESP-IDF logging to capture all ESP_LOG* output
     * - Logs to /sdcard/logs/system.log with timestamps
     * - Rotation and size limits configured in menuconfig
     */
    sd_logger_init();

    /* Initialize power manager
     * - Monitors face-down orientation via IMU
     * - Fades screen off after configurable timeout
     * - Enters light sleep after extended face-down time
     * - Wakes on touch or motion
     */
    power_manager_init();

    /*==========================================================================
     * PHASE 2: Content Discovery
     *=========================================================================*/

    /* Scan SD card for playable music files
     * Results stored for the music player app to use
     */
    LVGL_Search_Music();

    /*==========================================================================
     * PHASE 3: Phone UI Framework Setup
     *=========================================================================*/

    /* Lock LVGL mutex before any LVGL operations
     * Required for thread-safe access to LVGL objects
     */
    lvgl_port_lock(0);

    /* Create the phone object with the LVGL display handle
     * This is the main container for the phone-like UI
     */
    ESP_Brookesia_Phone *phone = new ESP_Brookesia_Phone(handles->lvgl_disp_handle);
    ESP_BROOKESIA_CHECK_NULL_EXIT(phone, "Create phone failed");

    /* Load and apply the 240x284 dark theme stylesheet
     * This defines colors, fonts, and layout for the phone UI
     */
    ESP_Brookesia_PhoneStylesheet_t *stylesheet = new ESP_Brookesia_PhoneStylesheet_t(ESP_BROOKESIA_PHONE_240_284_DARK_STYLESHEET());
    ESP_BROOKESIA_CHECK_NULL_EXIT(stylesheet, "Create stylesheet failed");

    ESP_LOGI(TAG, "Using stylesheet (%s)", stylesheet->core.name);
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->addStylesheet(stylesheet), "Add stylesheet failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->activateStylesheet(stylesheet), "Activate stylesheet failed");
    delete stylesheet;  /* Stylesheet data is copied, safe to delete */

    /* Configure touch input for the phone UI */
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->setTouchDevice(handles->lvgl_touch_indev_handle), "Set touch device failed");

    /* Register LVGL mutex callbacks for thread-safe operation
     * These are called by ESP-Brookesia when accessing LVGL from non-LVGL tasks
     */
    phone->registerLvLockCallback((ESP_Brookesia_GUI_LockCallback_t)(lvgl_port_lock), 0);
    phone->registerLvUnlockCallback((ESP_Brookesia_GUI_UnlockCallback_t)(lvgl_port_unlock));

    /* Start the phone UI (creates home screen, status bar, navigation bar) */
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->begin(), "Begin failed");

    /* Store phone pointer for WiFi status callback
     * This allows the WiFi manager callback to update the status bar icon
     */
    g_phone = phone;

    /* Set initial WiFi icon to disconnected state
     * Will be updated when WiFi connects via callback
     */
    phone->getHome().getStatusBar()->setWifiIconState(0);

    /*==========================================================================
     * PHASE 4: Application Installation
     *=========================================================================*/

    /* Install Music Player App
     * - Browse and play audio files from SD card
     * - Supports WAV, MP3 formats via ESP Audio Simple Player
     * Parameters: (use_status_bar=0, use_navigation_bar=0) - app manages its own UI
     */
    PhoneMusicConf *app_music_conf = new PhoneMusicConf(0, 0);
    ESP_BROOKESIA_CHECK_NULL_EXIT(app_music_conf, "Create app music failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT((phone->installApp(app_music_conf) >= 0), "Install app music failed");

    /* Install Settings App
     * - View system info (SD card size, flash size)
     * - Battery and power monitoring (AXP2101)
     * - WiFi scanning
     * - Backlight brightness control
     * - RTC time display
     */
    PhoneSettingConf *app_setting_conf = new PhoneSettingConf(0, 0);
    ESP_BROOKESIA_CHECK_NULL_EXIT(app_setting_conf, "Create app setting failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT((phone->installApp(app_setting_conf) >= 0), "Install app setting failed");

    /* Install Gyroscope Monitor App
     * - Real-time display of QMI8658 IMU data
     * - Shows accelerometer X/Y/Z, gyroscope X/Y/Z, temperature
     * - Updates at 100ms intervals
     */
    PhoneGyroscopeConf *app_gyroscope_conf = new PhoneGyroscopeConf(0, 0);
    ESP_BROOKESIA_CHECK_NULL_EXIT(app_gyroscope_conf, "Create app gyroscope failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT((phone->installApp(app_gyroscope_conf) >= 0), "Install app gyroscope failed");

    /* Install Audio Recorder App
     * - Record audio to WAV file on SD card
     * - Uses I2S RX channel for audio capture
     * - 5-second recording duration
     */
    PhoneRecConf *app_rec_conf = new PhoneRecConf(0, 0);
    ESP_BROOKESIA_CHECK_NULL_EXIT(app_rec_conf, "Create app rec failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT((phone->installApp(app_rec_conf) >= 0), "Install app rec failed");

    /* Install MiBuddy App
     * - Virtual buddy that displays images from SD card
     * - Loads PNG images from /sdcard/Images/ folder
     * - Cycles through images every 2 seconds in a loop
     * - Auto-launches as the startup app
     */
    PhoneMiBuddyConf *app_mibuddy_conf = new PhoneMiBuddyConf(1, 0);
    ESP_BROOKESIA_CHECK_NULL_EXIT(app_mibuddy_conf, "Create app mibuddy failed");
    int mibuddy_app_id = phone->installApp(app_mibuddy_conf);
    ESP_BROOKESIA_CHECK_FALSE_EXIT((mibuddy_app_id >= 0), "Install app mibuddy failed");

    /* Auto-launch MiBuddy as startup app */
    ESP_Brookesia_CoreAppEventData_t startup_event = {
        .id = mibuddy_app_id,
        .type = ESP_BROOKESIA_CORE_APP_EVENT_TYPE_START,
        .data = nullptr
    };
    phone->sendAppEvent(&startup_event);
    ESP_LOGI(TAG, "Auto-launched MiBuddy app (id=%d)", mibuddy_app_id);

    /*==========================================================================
     * PHASE 5: Background Services
     *=========================================================================*/

    /* Create 1-second timer to update status bar clock
     * The phone object is passed as user_data to the callback
     */
    ESP_BROOKESIA_CHECK_NULL_EXIT(
        lv_timer_create(on_clock_update_timer_cb, 1000, phone),
        "Create clock update timer failed"
    );

    /* Create 5-second timer to update battery status and icon color
     * Updates battery percentage and changes color:
     * - Red for critical (<5%)
     * - Orange for low (<20%)
     * - White for normal
     */
    ESP_BROOKESIA_CHECK_NULL_EXIT(
        lv_timer_create(on_battery_update_timer_cb, 5000, phone),
        "Create battery update timer failed"
    );

    /* Release LVGL mutex - UI is now ready and running */
    lvgl_port_unlock();

    /* Note: app_main returns here, but the LVGL task continues running
     * in the background, handling UI updates and touch events.
     * FreeRTOS scheduler manages all tasks automatically.
     */
}