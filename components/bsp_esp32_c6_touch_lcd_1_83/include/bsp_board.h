/**
 * @file bsp_board.h
 * @brief Board Support Package (BSP) for ESP32-C6 Touch LCD 1.83" board
 *
 * This header provides the hardware abstraction layer for all peripherals:
 * - Display (LCD via SPI)
 * - Touch controller (CST816S via I2C)
 * - Audio (I2S TX/RX)
 * - Sensors (QMI8658 IMU, PCF85063A RTC)
 * - Power management (AXP2101 PMU)
 * - Storage (SD card via SPI)
 * - Connectivity (WiFi, Bluetooth)
 *
 * Hardware Configuration:
 * - MCU: ESP32-C6
 * - Display: 240x284 pixels, 1.83" LCD
 * - Touch: CST816S capacitive touch controller
 * - Audio: Stereo I2S, 16kHz sample rate, 32-bit
 */

#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "pcf85063a.h"
#include "qmi8658.h"
#include "esp_lvgl_port.h"
#include "esp_wifi.h"
#include "esp_wifi.h"

/*===========================================================================
 * Power Management (AXP2101 PMU)
 *===========================================================================*/

/**
 * @brief Initialize AXP2101 Power Management Unit
 *
 * Configures power rails, battery charging, and power monitoring.
 * Must be called after I2C is initialized.
 *
 * @return ESP_OK on success
 */
esp_err_t AXP2101_driver_init(void);

/**
 * @brief PMU interrupt service routine handler
 *
 * Called when PMU generates an interrupt (battery events, button press, etc.)
 */
void pmu_isr_handler(void);

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * I2C Bus Configuration
 * Used for: Touch controller, RTC, IMU, PMU
 *===========================================================================*/

#define I2C_NUM         (0)             /**< I2C port number */
#define GPIO_I2C_SCL    (GPIO_NUM_8)    /**< I2C clock line */
#define GPIO_I2C_SDA    (GPIO_NUM_7)    /**< I2C data line */

/*===========================================================================
 * LCD Display Configuration (SPI)
 * Controller: ST7789-compatible, 240x284 pixels, 16-bit color
 *===========================================================================*/

#define LCD_SCK     1   /**< SPI clock */
#define LCD_DIN     2   /**< SPI MOSI (data in to LCD) */
#define LCD_CS      5   /**< SPI chip select */
#define LCD_DC      3   /**< Data/Command select */
#define LCD_RST     4   /**< Hardware reset */
#define LCD_BL      6   /**< Backlight PWM control */

/*===========================================================================
 * Touch Controller Configuration (CST816S via I2C)
 *===========================================================================*/

#define TOUCH_RST   -1  /**< Touch reset pin (-1 = not connected) */
#define TOUCH_INT   11  /**< Touch interrupt pin */

/*===========================================================================
 * LCD SPI and Display Parameters
 *===========================================================================*/

#define EXAMPLE_LCD_SPI_NUM         (SPI2_HOST)         /**< SPI host to use */
#define EXAMPLE_LCD_PIXEL_CLK_HZ    (40 * 1000 * 1000)  /**< SPI clock: 40MHz */
#define EXAMPLE_LCD_CMD_BITS        (8)                 /**< Command bit width */
#define EXAMPLE_LCD_PARAM_BITS      (8)                 /**< Parameter bit width */
#define EXAMPLE_LCD_BITS_PER_PIXEL  (16)                /**< RGB565 color format */
#define EXAMPLE_LCD_DRAW_BUFF_DOUBLE (0)                /**< Single buffer mode */
#define EXAMPLE_LCD_DRAW_BUFF_HEIGHT (30)               /**< Draw buffer height (lines) */
#define EXAMPLE_LCD_BL_ON_LEVEL     (1)                 /**< Backlight on = HIGH */
#define Backlight_MAX               100                 /**< Maximum backlight value */
#define DEFAULT_BACKLIGHT           90                  /**< Default backlight (90%) */

#define EXAMPLE_LCD_H_RES   (240)   /**< Horizontal resolution (pixels) */
#define EXAMPLE_LCD_V_RES   (284)   /**< Vertical resolution (pixels) */

/*===========================================================================
 * Backlight PWM (LEDC) Configuration
 *===========================================================================*/

#define LEDC_HS_TIMER          LEDC_TIMER_0             /**< Timer for PWM */
#define LEDC_LS_MODE           LEDC_LOW_SPEED_MODE      /**< Low speed mode */
#define LEDC_HS_CH0_GPIO       TEST_PIN_NUM_LCD_BK_LIGHT /**< Backlight GPIO */
#define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_0           /**< PWM channel */
#define LEDC_TEST_DUTY         (4000)                   /**< Initial duty cycle */
#define LEDC_ResolutionRatio   LEDC_TIMER_13_BIT        /**< 13-bit resolution */
#define LEDC_MAX_Duty          ((1 << LEDC_ResolutionRatio) - 1) /**< Max duty value */

/*===========================================================================
 * SD Card Configuration (SPI)
 * Shares SPI bus with LCD, uses separate CS pin
 *===========================================================================*/

#define FUNC_SDSPI_EN       (1)             /**< Enable SD card functionality */
#define SDSPI_HOST          (SPI2_HOST)     /**< SPI host (shared with LCD) */
#define GPIO_SDSPI_CS       (GPIO_NUM_17)   /**< SD card chip select */
#define GPIO_SDSPI_SCLK     (GPIO_NUM_1)    /**< SPI clock (shared with LCD) */
#define GPIO_SDSPI_MISO     (GPIO_NUM_16)   /**< SPI MISO (SD card data out) */
#define GPIO_SDSPI_MOSI     (GPIO_NUM_2)    /**< SPI MOSI (shared with LCD) */

#define MOUNT_POINT         "/sdcard"       /**< SD card mount point in VFS */
#define EXAMPLE_MAX_CHAR_SIZE    64         /**< Max chars for display strings */
#define MAX_FILE_NAME_SIZE  100             /**< Max file name length */
#define MAX_PATH_SIZE       512             /**< Max full path length */

/*===========================================================================
 * I2S Audio Configuration
 * Stereo audio at 44.1kHz, 32-bit samples (standard for MP3 playback)
 *===========================================================================*/

#define I2S_NUM             I2S_NUM_0       /**< I2S port number */
#define I2S_SAMPLE_RATE     44100           /**< Sample rate: 44.1kHz for MP3 playback */
#define I2S_CHANNEL_FORMAT  2               /**< Stereo (2 channels) */
#define I2S_BITS_PER_CHAN   32              /**< 32 bits per channel */
#define GPIO_I2S_LRCK       (GPIO_NUM_22)   /**< Word select (L/R clock) */
#define GPIO_I2S_MCLK       (GPIO_NUM_19)   /**< Master clock */
#define GPIO_I2S_SCLK       (GPIO_NUM_20)   /**< Bit clock */
#define GPIO_I2S_SDIN       (GPIO_NUM_21)   /**< Serial data in (from mic) */
#define GPIO_I2S_DOUT       (GPIO_NUM_23)   /**< Serial data out (to speaker) */

/*===========================================================================
 * Audio Volume Configuration
 *===========================================================================*/

#define RECORD_VOLUME   (50.0)  /**< Recording gain (0-100) */
#define PLAYER_VOLUME   (95)    /**< Default playback volume (0-100) */

/*===========================================================================
 * Power Control
 *===========================================================================*/

#define GPIO_PWR_CTRL       (-1)    /**< Power control GPIO (-1 = not used) */
#define GPIO_PWR_ON_LEVEL   (1)     /**< Active high power enable */

/*===========================================================================
 * I2S Configuration Macros
 * Version-dependent configuration for ESP-IDF 5.x vs 4.x
 *===========================================================================*/

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    /**
     * @brief Default I2S configuration for ESP-IDF 5.x
     *
     * Uses the new I2S driver API with standard mode and Philips format.
     * Configured for stereo, 32-bit samples.
     */
    #define I2S_CONFIG_DEFAULT(sample_rate, channel_fmt, bits_per_chan) { \
            .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate), \
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(32, I2S_SLOT_MODE_STEREO), \
            .gpio_cfg = { \
                .mclk = GPIO_I2S_MCLK, \
                .bclk = GPIO_I2S_SCLK, \
                .ws   = GPIO_I2S_LRCK, \
                .dout = GPIO_I2S_DOUT, \
                .din  = GPIO_I2S_SDIN, \
            }, \
        }

#else
    /**
     * @brief Default I2S configuration for ESP-IDF 4.x (legacy)
     *
     * Uses the legacy I2S driver API.
     * Configured as master, full-duplex (RX+TX), stereo, 32-bit.
     */
    #define I2S_CONFIG_DEFAULT(sample_rate, channel_fmt, bits_per_chan) { \
        .mode                   = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX, \
        .sample_rate            = 16000, \
        .bits_per_sample        = I2S_BITS_PER_SAMPLE_32BIT, \
        .channel_format         = I2S_CHANNEL_FMT_RIGHT_LEFT, \
        .communication_format   = I2S_COMM_FORMAT_STAND_I2S, \
        .intr_alloc_flags       = ESP_INTR_FLAG_LEVEL1, \
        .dma_buf_count          = 6, \
        .dma_buf_len            = 160, \
        .use_apll               = false, \
        .tx_desc_auto_clear     = true, \
        .fixed_mclk             = 0, \
        .mclk_multiple          = I2S_MCLK_MULTIPLE_DEFAULT, \
        .bits_per_chan          = I2S_BITS_PER_CHAN_32BIT, \
    }

#endif

/*===========================================================================
 * BSP Handles Structure
 * Contains handles to all initialized peripherals
 *===========================================================================*/

/**
 * @brief BSP handles structure
 *
 * This structure holds handles to all initialized BSP components,
 * allowing applications to access hardware peripherals.
 */
typedef struct {
    esp_lcd_panel_handle_t    panel;                  /**< LCD panel handle */
    esp_lcd_panel_io_handle_t io;                     /**< LCD IO handle */
    esp_lcd_touch_handle_t    tp_handle;              /**< Touch panel handle */
    i2s_chan_handle_t         i2s_tx_handle;          /**< I2S TX channel (speaker) */
    i2s_chan_handle_t         i2s_rx_handle;          /**< I2S RX channel (microphone) */
    lv_display_t              *lvgl_disp_handle;      /**< LVGL display handle */
    lv_indev_t                *lvgl_touch_indev_handle; /**< LVGL touch input device */
    qmi8658_dev_t             *qmi8658_dev;           /**< QMI8658 IMU device handle */
} bsp_handles_t;

/*===========================================================================
 * LCD Driver API
 *===========================================================================*/

/**
 * @brief Initialize LCD display driver
 * @return ESP_OK on success
 */
esp_err_t bsp_lcd_driver_init(void);

/**
 * @brief Set LCD backlight brightness
 * @param light Brightness level (0-100)
 */
void bsp_set_backlight(uint8_t light);

/**
 * @brief Get current backlight brightness
 * @return Current brightness level (0-100)
 */
uint8_t bsp_read_backlight_value(void);

/**
 * @brief Fade backlight to target brightness over duration
 * @param target Target brightness level (0-100)
 * @param duration_ms Fade duration in milliseconds
 */
void bsp_fade_backlight(uint8_t target, uint32_t duration_ms);

/*===========================================================================
 * Touch Driver API
 *===========================================================================*/

/**
 * @brief Initialize touch controller (CST816S)
 * @return ESP_OK on success
 */
esp_err_t bsp_touch_driver_init(void);

/*===========================================================================
 * I2C Driver API
 *===========================================================================*/

/**
 * @brief Initialize I2C master bus
 * @return ESP_OK on success
 */
esp_err_t bsp_i2c_master_init(void);

/*===========================================================================
 * I2S Audio Driver API
 *===========================================================================*/

/**
 * @brief Initialize I2S audio interface (TX and RX)
 * @return ESP_OK on success
 */
esp_err_t bsp_i2s_init(void);

/**
 * @brief Deinitialize I2S audio interface
 * @return ESP_OK on success
 */
esp_err_t bsp_i2s_deinit(void);

/*===========================================================================
 * Audio Codec API
 *===========================================================================*/

/**
 * @brief Initialize audio codec
 * @return ESP_OK on success
 */
esp_err_t bsp_codec_init(void);

/**
 * @brief Deinitialize audio codec
 * @return ESP_OK on success
 */
esp_err_t bsp_codec_deinit(void);

/**
 * @brief Set playback volume
 * @param volume Volume level (0-100)
 * @return ESP_OK on success
 */
esp_err_t esp_audio_set_play_vol(int volume);

/**
 * @brief Get current playback volume
 * @param[out] volume Pointer to store volume level
 * @return ESP_OK on success
 */
esp_err_t esp_audio_get_play_vol(int *volume);

/**
 * @brief Play audio data through I2S
 * @param data Audio sample buffer (16-bit PCM)
 * @param length Buffer length in bytes
 * @param ticks_to_wait Timeout in ticks
 * @return ESP_OK on success
 */
esp_err_t esp_audio_play(const int16_t* data, int length, uint32_t ticks_to_wait);

/**
 * @brief Get audio feed data from microphone
 * @param is_get_raw_channel Get raw channel data flag
 * @param[out] buffer Buffer to store audio samples
 * @param buffer_len Buffer length
 * @return ESP_OK on success
 */
esp_err_t esp_get_feed_data(bool is_get_raw_channel, int16_t *buffer, int buffer_len);

/**
 * @brief Get number of audio feed channels
 * @return Number of channels
 */
int esp_get_feed_channel(void);

/**
 * @brief Get audio input format string
 * @return Format string
 */
char* esp_get_input_format(void);

/*===========================================================================
 * Battery API (AXP2101)
 *===========================================================================*/

/**
 * @brief Get battery percentage
 * @return Battery level 0-100%, or -1 if no battery connected
 */
int bsp_battery_get_percent(void);

/**
 * @brief Check if battery is charging
 * @return true if charging, false otherwise
 */
bool bsp_battery_is_charging(void);

/*===========================================================================
 * Board Initialization API
 *===========================================================================*/

/**
 * @brief Get BSP handles structure
 * @return Pointer to BSP handles (contains all peripheral handles)
 */
bsp_handles_t *bsp_display_get_handles(void);

/**
 * @brief Initialize all board peripherals
 *
 * Initializes: I2C, LCD, touch, I2S, SD card, RTC, IMU, LVGL
 * Call this first in app_main().
 *
 * @return ESP_OK on success
 */
esp_err_t bsp_init(void);

/*===========================================================================
 * SD Card Driver API
 *===========================================================================*/

/**
 * @brief Initialize SD card filesystem
 * @return ESP_OK on success
 */
esp_err_t sd_card_init(void);

/**
 * @brief Get total SD card size
 * @return Size in MB, 0 if no card mounted
 */
uint32_t get_sdcard_total_size(void);

/**
 * @brief Search folder for files with specific extension
 * @param directory Directory path to search
 * @param fileExtension File extension to match (e.g., ".mp3")
 * @param[out] File_Name Array to store found file names
 * @param maxFiles Maximum number of files to find
 * @return Number of files found
 */
uint16_t Folder_retrieval(const char* directory, const char* fileExtension,
                          char File_Name[][MAX_FILE_NAME_SIZE], uint16_t maxFiles);

/*===========================================================================
 * IMU Driver API (QMI8658)
 *===========================================================================*/

/**
 * @brief Initialize QMI8658 IMU sensor
 * @return ESP_OK on success
 */
esp_err_t qmi8658_driver_init(void);

/*===========================================================================
 * RTC Driver API (PCF85063A)
 *===========================================================================*/

/**
 * @brief Initialize PCF85063A RTC
 * @return ESP_OK on success
 */
esp_err_t pcf85063a_driver_init(void);

/**
 * @brief Read current time from RTC
 * @param[out] time Pointer to datetime structure
 */
void get_rtc_data_to_str(pcf85063a_datetime_t *time);

/**
 * @brief Set RTC time
 * @param[in] time Pointer to datetime structure with new time
 * @return ESP_OK on success
 */
esp_err_t set_rtc_time(pcf85063a_datetime_t *time);

/*===========================================================================
 * LVGL Display API
 *===========================================================================*/

/**
 * @brief Initialize LVGL display driver
 * @return ESP_OK on success
 */
esp_err_t lvgl_driver_init(void);

/*===========================================================================
 * WiFi API
 *===========================================================================*/

/**
 * @brief Initialize WiFi and connect to AP
 * @param ssid WiFi network SSID
 * @param pass WiFi password
 */
void esp_wifi_port_init(const char *ssid, const char *pass);

/**
 * @brief Get current IP address
 * @param[out] ip Buffer to store IP address string
 */
void esp_wifi_port_get_ip(char *ip);

/**
 * @brief Scan for available WiFi networks
 * @param[out] ap_info Array to store AP information
 * @param[out] scan_number Number of APs found
 * @param scan_max_num Maximum APs to find
 * @return true on success
 */
bool esp_wifi_port_scan(wifi_ap_record_t *ap_info, uint16_t *scan_number, uint16_t scan_max_num);

/**
 * @brief Disconnect from WiFi network
 * @return ESP_OK on success
 */
esp_err_t esp_wifi_port_disconnect(void);

/**
 * @brief Connect to configured WiFi network
 * @return ESP_OK on success
 */
esp_err_t esp_wifi_port_connect(void);

/**
 * @brief Deinitialize WiFi
 */
void esp_wifi_port_deinit(void);

/*===========================================================================
 * Bluetooth API
 *===========================================================================*/

/**
 * @brief Initialize Bluetooth
 */
void bsp_bt_port_init(void);

#ifdef __cplusplus
}
#endif


