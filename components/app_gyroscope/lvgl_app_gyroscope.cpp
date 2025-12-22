/**
 * @file lvgl_app_gyroscope.cpp
 * @brief Gyroscope/IMU Sensor Monitor Application for ESP-Brookesia Phone UI
 *
 * This application displays real-time sensor data from the QMI8658 6-axis IMU:
 * - Accelerometer X, Y, Z (m/s²)
 * - Gyroscope X, Y, Z (rad/s)
 * - Temperature (°C)
 * - Timestamp
 *
 * UI Layout:
 * - Left column: Accelerometer data (cyan) + Temperature (green)
 * - Right column: Gyroscope data (orange) + Timestamp (purple)
 * - Dark background for contrast
 *
 * Updates at 100ms intervals for smooth real-time display.
 */

#include "lvgl_app_gyroscope.hpp"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "private/esp_brookesia_utils.h"

#include "app_gyroscope_assets.h"
#include "bsp_board.h"

using namespace std;
using namespace esp_brookesia::gui;

/*===========================================================================
 * Module State Variables
 *===========================================================================*/

static lv_timer_t * auto_step_timer = NULL;     /**< Sensor update timer */

/**
 * @brief Label array for sensor data display
 *
 * Index mapping:
 * - 0: Accelerometer X
 * - 1: Accelerometer Y
 * - 2: Accelerometer Z
 * - 3: Temperature
 * - 4: Gyroscope X
 * - 5: Gyroscope Y
 * - 6: Gyroscope Z
 * - 7: Timestamp
 */
static lv_obj_t *labels[8];

static bsp_handles_t *handles;                  /**< BSP handles for sensor access */
static void example1_increase_lvgl_tick(lv_timer_t * t);


/*===========================================================================
 * Constructors/Destructor
 *===========================================================================*/

/**
 * @brief Construct gyroscope app with status/navigation bar options
 */
PhoneGyroscopeConf::PhoneGyroscopeConf(bool use_status_bar, bool use_navigation_bar):
    ESP_Brookesia_PhoneApp("Gyroscope", &icon_gyroscope, true, use_status_bar, use_navigation_bar)
{
}

/**
 * @brief Construct gyroscope app with default settings
 */
PhoneGyroscopeConf::PhoneGyroscopeConf():
    ESP_Brookesia_PhoneApp("Gyroscope", &icon_gyroscope, true)
{
}

/**
 * @brief Destructor
 */
PhoneGyroscopeConf::~PhoneGyroscopeConf()
{
    ESP_BROOKESIA_LOGD("Destroy(@0x%p)", this);
}

/*===========================================================================
 * UI Layout
 *===========================================================================*/

/**
 * @brief Initialize sensor display layout
 *
 * Creates a two-column layout:
 * - Left column: Accelerometer X/Y/Z (cyan) + Temperature (green)
 * - Right column: Gyroscope X/Y/Z (orange) + Timestamp (purple)
 *
 * Screen configuration:
 * - 240x284 pixels
 * - Dark background (0x121212)
 * - Scrolling disabled
 * - Montserrat 14pt font
 */
void sensor_layout_init(void) {
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_size(screen, 240, 284);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x121212), 0);  // 深色背景
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);      // 禁用滚动
    lv_obj_set_scroll_dir(screen, LV_DIR_NONE);

    // 1. 标题
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Sensor Monitor");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    // 2. 布局参数（确保内容不溢出）
    lv_coord_t left_col_x = 10;        // 左列X起点
    lv_coord_t right_col_x = 240 - 110; // 右列X起点（右侧预留10px边距）
    lv_coord_t top_y = 40;             // 数据区顶部Y起点
    lv_coord_t item_h = 38;            // 每个数据项高度
    lv_coord_t item_spacing = 3;       // 数据项间距

    // 3. 左侧数据区（加速度X/Y/Z、温度）
    // 3.1 加速度X
    labels[0] = lv_label_create(screen);
    lv_obj_set_style_text_color(labels[0], lv_color_hex(0x00FFFF), 0);  // 青色
    lv_obj_set_style_text_font(labels[0], &lv_font_montserrat_14, 0);
    lv_label_set_text(labels[0], "Accel X:\n--.- m/s²");
    lv_obj_align(labels[0], LV_ALIGN_TOP_LEFT, left_col_x, top_y);

    // 3.2 加速度Y
    labels[1] = lv_label_create(screen);
    lv_obj_set_style_text_color(labels[1], lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_text_font(labels[1], &lv_font_montserrat_14, 0);
    lv_label_set_text(labels[1], "Accel Y:\n--.- m/s²");
    lv_obj_align(labels[1], LV_ALIGN_TOP_LEFT, left_col_x, top_y + item_h + item_spacing);

    // 3.3 加速度Z
    labels[2] = lv_label_create(screen);
    lv_obj_set_style_text_color(labels[2], lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_text_font(labels[2], &lv_font_montserrat_14, 0);
    lv_label_set_text(labels[2], "Accel Z:\n--.- m/s²");
    lv_obj_align(labels[2], LV_ALIGN_TOP_LEFT, left_col_x, top_y + 2*(item_h + item_spacing));

    // 4.4 温度（左侧第四项）
    labels[3] = lv_label_create(screen);
    lv_obj_set_style_text_color(labels[3], lv_color_hex(0x00FF00), 0);  // 绿色
    lv_obj_set_style_text_font(labels[3], &lv_font_montserrat_14, 0);
    lv_label_set_text(labels[3], "Temp:\n--.- °C");
    lv_obj_align(labels[3], LV_ALIGN_TOP_LEFT, left_col_x, top_y + 3*(item_h + item_spacing));

    // 5. 右侧数据区（陀螺仪X/Y/Z、时间戳）
    // 5.1 陀螺仪X
    labels[4] = lv_label_create(screen);
    lv_obj_set_style_text_color(labels[4], lv_color_hex(0xFFAA00), 0);  // 橙色
    lv_obj_set_style_text_font(labels[4], &lv_font_montserrat_14, 0);
    lv_label_set_text(labels[4], "Gyro X:\n--.- rad/s");
    lv_obj_align(labels[4], LV_ALIGN_TOP_LEFT, right_col_x, top_y);

    // 5.2 陀螺仪Y
    labels[5] = lv_label_create(screen);
    lv_obj_set_style_text_color(labels[5], lv_color_hex(0xFFAA00), 0);
    lv_obj_set_style_text_font(labels[5], &lv_font_montserrat_14, 0);
    lv_label_set_text(labels[5], "Gyro Y:\n--.- rad/s");
    lv_obj_align(labels[5], LV_ALIGN_TOP_LEFT, right_col_x, top_y + item_h + item_spacing);

    // 5.3 陀螺仪Z
    labels[6] = lv_label_create(screen);
    lv_obj_set_style_text_color(labels[6], lv_color_hex(0xFFAA00), 0);
    lv_obj_set_style_text_font(labels[6], &lv_font_montserrat_14, 0);
    lv_label_set_text(labels[6], "Gyro Z:\n--.- rad/s");
    lv_obj_align(labels[6], LV_ALIGN_TOP_LEFT, right_col_x, top_y + 2*(item_h + item_spacing));

    // 5.4 时间戳（右侧第四项）
    labels[7] = lv_label_create(screen);
    lv_obj_set_style_text_color(labels[7], lv_color_hex(0xBB88FF), 0);  // 紫色
    lv_obj_set_style_text_font(labels[7], &lv_font_montserrat_14, 0);
    lv_label_set_text(labels[7], "Time:\n---- ms");
    lv_obj_align(labels[7], LV_ALIGN_TOP_LEFT, right_col_x, top_y + 3*(item_h + item_spacing));
}

/*===========================================================================
 * App Lifecycle Methods
 *===========================================================================*/

/**
 * @brief Called when the gyroscope app is launched
 *
 * Initializes the sensor display layout and starts a 100ms timer
 * to continuously update sensor readings.
 *
 * @return true on success
 */
bool PhoneGyroscopeConf::run(void)
{
    ESP_BROOKESIA_LOGD("Run");

    /* Get BSP handles for sensor access */
    handles = bsp_display_get_handles();

    /* Create sensor display layout */
    sensor_layout_init();

    /* Start 100ms update timer for real-time display */
    auto_step_timer = lv_timer_create(example1_increase_lvgl_tick, 100, NULL);

    return true;
}

/**
 * @brief Handle back button press
 *
 * Closes the app and returns to home screen.
 *
 * @return true on success
 */
bool PhoneGyroscopeConf::back(void)
{
    ESP_BROOKESIA_LOGD("Back");

    /* Notify core to close the app */
    ESP_BROOKESIA_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

/*===========================================================================
 * Sensor Update
 *===========================================================================*/

static char buf[7][30];  /**< String buffers for formatted sensor values */

/**
 * @brief Timer callback to update sensor display
 *
 * Called every 100ms to read QMI8658 IMU data and update labels.
 * Reads accelerometer, gyroscope, and temperature values.
 *
 * @param t LVGL timer handle (unused)
 */
static void example1_increase_lvgl_tick(lv_timer_t * t)
{
    qmi8658_data_t data;
    bool ready;

    /* Check if new sensor data is available */
    esp_err_t ret = qmi8658_is_data_ready(handles->qmi8658_dev, &ready);
    if (ret == ESP_OK && ready) {
        /* Read all sensor data */
        ret = qmi8658_read_sensor_data(handles->qmi8658_dev, &data);
        if (ret == ESP_OK) {
            /* Update accelerometer labels (left column) */
            sprintf(buf[0], "%.4f", data.accelX);
            sprintf(buf[1], "%.4f", data.accelY);
            sprintf(buf[2], "%.4f", data.accelZ);
            sprintf(buf[3], "%.4f", data.temperature);
            lv_label_set_text_fmt(labels[0], "Accel X:\n %s", buf[0]);
            lv_label_set_text_fmt(labels[1], "Accel Y:\n %s", buf[1]);
            lv_label_set_text_fmt(labels[2], "Accel Z:\n %s", buf[2]);
            lv_label_set_text_fmt(labels[3], "Temp:\n %s °C", buf[3]);

            /* Update gyroscope labels (right column) */
            sprintf(buf[4], "%.4f", data.gyroX);
            sprintf(buf[5], "%.4f", data.gyroY);
            sprintf(buf[6], "%.4f", data.gyroZ);
            lv_label_set_text_fmt(labels[4], "Gyro X:\n %s", buf[4]);
            lv_label_set_text_fmt(labels[5], "Gyro Y:\n %s", buf[5]);
            lv_label_set_text_fmt(labels[6], "Gyro Z:\n %s", buf[6]);
            lv_label_set_text_fmt(labels[7], "Timestamp:\n %lu ", data.timestamp);
        }
    }
}

/**
 * @brief Called when the app is closed
 *
 * @return true on success
 */
bool PhoneGyroscopeConf::close(void)
{
    ESP_BROOKESIA_LOGD("Close");

    /* Notify core that app is closing */
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
