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

using namespace std;
using namespace esp_brookesia::gui;

/*===========================================================================
 * UI Element References
 *===========================================================================*/

static lv_style_t style_text_muted;         /**< Muted text style */

/* Main panel elements */
static lv_obj_t * SD_Size;                  /**< SD card size text area */
static lv_obj_t * FlashSize;                /**< Flash size text area */
static lv_obj_t * RTC_Time;                 /**< RTC time display */
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

    lv_obj_t * panel1 = lv_obj_create(lv_screen_active());
    lv_obj_set_height(panel1, LV_SIZE_CONTENT);
    lv_obj_t * panel1_title = lv_label_create(panel1);
    lv_label_set_text(panel1_title, "Onboard parameter");

    lv_obj_t * SD_label = lv_label_create(panel1);
    lv_label_set_text(SD_label, "SD Card");
    SD_Size = lv_textarea_create(panel1);
    lv_textarea_set_one_line(SD_Size, true);
    lv_textarea_set_placeholder_text(SD_Size, "SD Size");
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

    lv_obj_t * bat_label = lv_label_create(panel1);
    lv_label_set_text(bat_label, "battery");
    lv_obj_add_style(bat_label, &style_text_muted, 0);
    but_bat_msg = lv_button_create(panel1);
    lv_obj_set_size(but_bat_msg,200,35);
    lv_obj_add_event_cb(but_bat_msg, bat_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * bat_msg_label = lv_label_create(but_bat_msg);
    lv_obj_align(bat_msg_label,LV_ALIGN_CENTER,0,0);
    lv_label_set_text(bat_msg_label, "bat Message");
    lv_obj_add_style(bat_msg_label, &style_text_muted, 0);

    lv_obj_t * wifi_label = lv_label_create(panel1);
    lv_label_set_text(wifi_label, "wifi");
    lv_obj_add_style(wifi_label, &style_text_muted, 0);
    but_wifi_msg = lv_button_create(panel1);
    lv_obj_set_size(but_wifi_msg,200,35);
    lv_obj_add_event_cb(but_wifi_msg, wifi_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * wifi_msg_label = lv_label_create(but_wifi_msg);
    lv_obj_align(wifi_msg_label,LV_ALIGN_CENTER,0,0);
    lv_label_set_text(wifi_msg_label, "get wifi Message");
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
        LV_GRID_CONTENT,  /*8: bat label*/
        40,               /*9: bat button*/
        LV_GRID_CONTENT,  /*10: wifi label*/
        40,               /*11: wifi button*/
        LV_GRID_CONTENT,  /*12: Backlight label*/
        40,               /*13: Backlight slider*/
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

    // 电池模块（行8-9）
    lv_obj_set_grid_cell(bat_label, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_START, 8, 1);
    lv_obj_set_grid_cell(but_bat_msg, LV_GRID_ALIGN_STRETCH, 0, 5, LV_GRID_ALIGN_CENTER, 9, 1);

    // WiFi模块（行10-11）
    lv_obj_set_grid_cell(wifi_label, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_START, 10, 1);
    lv_obj_set_grid_cell(but_wifi_msg, LV_GRID_ALIGN_STRETCH, 0, 5, LV_GRID_ALIGN_CENTER, 11, 1);

    // 背光模块（行12-13）
    lv_obj_set_grid_cell(Backlight_label, LV_GRID_ALIGN_START, 0, 5, LV_GRID_ALIGN_START, 12, 1);
    lv_obj_set_grid_cell(Backlight_slider, LV_GRID_ALIGN_STRETCH, 0, 5, LV_GRID_ALIGN_CENTER, 13, 1);


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

    /* Update NTP sync button state based on WiFi connection */
    if (but_ntp_sync != NULL) {
        if (wifi_manager_is_connected()) {
            lv_obj_clear_state(but_ntp_sync, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(but_ntp_sync, LV_STATE_DISABLED);
        }
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
