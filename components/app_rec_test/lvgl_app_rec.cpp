/**
 * @file lvgl_app_rec.cpp
 * @brief Audio Recorder Application for ESP-Brookesia Phone UI
 *
 * This application provides audio recording functionality:
 * - Record audio from I2S microphone
 * - Save as WAV file to SD card
 * - Automatic playback after recording completes
 *
 * Recording Parameters:
 * - Format: PCM WAV
 * - Sample rate: 16kHz
 * - Bit depth: 32-bit
 * - Channels: Stereo (2)
 * - Duration: 5 seconds
 * - Output: /sdcard/Recordings/RECORD.WAV
 *
 * The recording runs in a separate FreeRTOS task to avoid blocking the UI.
 */

#include "lvgl_app_rec.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "private/esp_brookesia_utils.h"
#include "esp_check.h"
#include "app_rec.h"
#include "bsp_board.h"
#include "audio_driver.h"
#include <sys/stat.h>

static const char *TAG = "app_rec";

/*===========================================================================
 * Recording Configuration
 *===========================================================================*/

#define EXAMPLE_I2S_TDM_FORMAT     (ES7210_I2S_FMT_I2S)
#define EXAMPLE_I2S_CHAN_NUM       (2)          /**< Stereo recording */
#define EXAMPLE_I2S_SAMPLE_RATE    (16000)      /**< 16kHz sample rate */
#define EXAMPLE_I2S_MCLK_MULTIPLE  (I2S_MCLK_MULTIPLE_256)
#define EXAMPLE_I2S_SAMPLE_BITS    (I2S_DATA_BIT_WIDTH_32BIT)  /**< 32-bit samples */
#define EXAMPLE_I2S_TDM_SLOT_MASK  (I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3)

/* SD card and recording output configuration */
#define EXAMPLE_RECORD_TIME_SEC    (5)              /**< Recording duration in seconds */
#define EXAMPLE_SD_MOUNT_POINT     "/sdcard"        /**< SD card mount point */
#define EXAMPLE_RECORDINGS_DIR     "/Recordings"    /**< Recordings directory */
#define EXAMPLE_RECORD_FILE_PATH   "/Recordings/RECORD.WAV"  /**< Output file path */

/*===========================================================================
 * WAV File Format Structures
 *===========================================================================*/

/**
 * @brief WAV file header structure
 *
 * Standard PCM WAV header with RIFF descriptor, format chunk, and data chunk.
 * Used to write proper WAV files that can be played by standard audio software.
 */
typedef struct {
    struct {
        char chunk_id[4];       /**< "RIFF" in ASCII */
        uint32_t chunk_size;    /**< Size of file minus 8 bytes */
        char chunk_format[4];   /**< "WAVE" in ASCII */
    } descriptor_chunk;         /**< RIFF header */
    struct {
        char subchunk_id[4];    /**< "fmt " in ASCII (note the space) */
        uint32_t subchunk_size; /**< Size of fmt chunk (16 for PCM) */
        uint16_t audio_format;  /**< Audio format (1 = PCM) */
        uint16_t num_of_channels; /**< Number of channels (1=mono, 2=stereo) */
        uint32_t sample_rate;   /**< Sample rate (e.g., 16000) */
        uint32_t byte_rate;     /**< Bytes per second */
        uint16_t block_align;   /**< Bytes per sample frame */
        uint16_t bits_per_sample; /**< Bits per sample (8, 16, 32) */
    } fmt_chunk;                /**< Format specification chunk */
    struct {
        char subchunk_id[4];    /**< "data" in ASCII */
        uint32_t subchunk_size; /**< Size of audio data */
        int16_t data[0];        /**< Audio samples (flexible array) */
    } data_chunk;               /**< Audio data chunk */
} wav_header_t;

/**
 * @brief Default header for PCM format WAV files
 *
 */
#define WAV_HEADER_PCM_DEFAULT(wav_sample_size, wav_sample_bits, wav_sample_rate, wav_channel_num) { \
    .descriptor_chunk = { \
        .chunk_id = {'R', 'I', 'F', 'F'}, \
        .chunk_size = (wav_sample_size) + sizeof(wav_header_t) - 8, \
        .chunk_format = {'W', 'A', 'V', 'E'} \
    }, \
    .fmt_chunk = { \
        .subchunk_id = {'f', 'm', 't', ' '}, \
        .subchunk_size = 16, /* 16 for PCM */ \
        .audio_format = 1, /* 1 for PCM */ \
        .num_of_channels = (wav_channel_num), \
        .sample_rate = (wav_sample_rate), \
        .byte_rate = (wav_sample_bits) * (wav_sample_rate) * (wav_channel_num) / 8, \
        .block_align = (wav_sample_bits) * (wav_channel_num) / 8, \
        .bits_per_sample = (wav_sample_bits)\
    }, \
    .data_chunk = { \
        .subchunk_id = {'d', 'a', 't', 'a'}, \
        .subchunk_size = (wav_sample_size) \
    } \
}



using namespace std;
using namespace esp_brookesia::gui;

/*===========================================================================
 * Module State Variables
 *===========================================================================*/

static lv_timer_t * auto_step_timer = NULL; /**< Unused timer (reserved) */
static lv_obj_t *labels[8];                 /**< Unused label array (reserved) */
static bsp_handles_t *handles;              /**< BSP handles for I2S access */
static void example1_increase_lvgl_tick(lv_timer_t * t);
static lv_obj_t * btn1;                     /**< Start recording button */
static TaskHandle_t task_handle = NULL;     /**< Recording task handle */
static lv_obj_t * msg_content_label = NULL; /**< Recording progress label */
static lv_obj_t * rec_msg;                  /**< Recording message box */

/*===========================================================================
 * Constructors/Destructor
 *===========================================================================*/

/**
 * @brief Construct recorder app with status/navigation bar options
 */
PhoneRecConf::PhoneRecConf(bool use_status_bar, bool use_navigation_bar):
    ESP_Brookesia_PhoneApp("rec", &icon_rec, true, use_status_bar, use_navigation_bar)
{
}

/**
 * @brief Construct recorder app with default settings
 */
PhoneRecConf::PhoneRecConf():
    ESP_Brookesia_PhoneApp("rec", &icon_rec, true)
{
}

/**
 * @brief Destructor
 */
PhoneRecConf::~PhoneRecConf()
{
    ESP_BROOKESIA_LOGD("Destroy(@0x%p)", this);
}





/*===========================================================================
 * Recording Functions
 *===========================================================================*/

/**
 * @brief Record audio to WAV file on SD card
 *
 * Records audio from I2S microphone for EXAMPLE_RECORD_TIME_SEC seconds
 * and saves as WAV file. Shows progress in UI and plays back after completion.
 *
 * @param i2s_rx_chan I2S receive channel handle
 * @return ESP_OK on success
 */
static esp_err_t record_wav(i2s_chan_handle_t i2s_rx_chan)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(i2s_rx_chan, ESP_FAIL, TAG, "invalid i2s channel handle pointer");
    

    uint32_t byte_rate = EXAMPLE_I2S_SAMPLE_RATE * EXAMPLE_I2S_CHAN_NUM * EXAMPLE_I2S_SAMPLE_BITS / 8;
    uint32_t wav_size = byte_rate * EXAMPLE_RECORD_TIME_SEC;

    const wav_header_t wav_header =
        WAV_HEADER_PCM_DEFAULT(wav_size, EXAMPLE_I2S_SAMPLE_BITS, EXAMPLE_I2S_SAMPLE_RATE, EXAMPLE_I2S_CHAN_NUM);

    lvgl_port_lock(0);

    /* Create Recordings directory if it doesn't exist */
    struct stat st;
    if (stat(EXAMPLE_SD_MOUNT_POINT EXAMPLE_RECORDINGS_DIR, &st) != 0) {
        ESP_LOGI(TAG, "Creating directory %s", EXAMPLE_RECORDINGS_DIR);
        mkdir(EXAMPLE_SD_MOUNT_POINT EXAMPLE_RECORDINGS_DIR, 0755);
    }

    ESP_LOGI(TAG, "Opening file %s", EXAMPLE_RECORD_FILE_PATH);
    FILE *f = fopen(EXAMPLE_SD_MOUNT_POINT EXAMPLE_RECORD_FILE_PATH, "w");
    ESP_RETURN_ON_FALSE(f, ESP_FAIL, TAG, "error while opening wav file");

    /* Write wav header */
    fwrite(&wav_header, sizeof(wav_header_t), 1, f);
    lvgl_port_unlock();

    /* Start recording */
    size_t wav_written = 0;
    static int16_t i2s_readraw_buff[4096];

    i2s_channel_enable(i2s_rx_chan);
    while (wav_written < wav_size) {
        if (wav_written % byte_rate < sizeof(i2s_readraw_buff)) {
            lvgl_port_lock(0);
            lv_label_set_text_fmt(msg_content_label,"Recording: %"PRIu32"/%ds",wav_written / byte_rate + 1, (int)EXAMPLE_RECORD_TIME_SEC);
            lvgl_port_unlock();
            //ESP_LOGI(TAG, "Recording: %"PRIu32"/%ds", wav_written / byte_rate + 1, (int)EXAMPLE_RECORD_TIME_SEC);
        }
        size_t bytes_read = 0;
        /* Read RAW samples from ES7210 */
        i2s_channel_read(i2s_rx_chan, i2s_readraw_buff, sizeof(i2s_readraw_buff), &bytes_read,pdMS_TO_TICKS(1000));
        /* Write the samples to the WAV file */
        lvgl_port_lock(0);
        fwrite(i2s_readraw_buff, bytes_read, 1, f);
        lvgl_port_unlock();
        wav_written += bytes_read;
    }

    

    
    lvgl_port_lock(0);
    fclose(f);
    lvgl_port_unlock();
    Audio_Play_Music("file:///sdcard/Recordings/RECORD.WAV");
    
    //Audio_Play_Music("file://sdcard/1.wav");

    return ret;
}


/**
 * @brief Recording task function (FreeRTOS task)
 *
 * Runs in background to avoid blocking UI during recording.
 *
 * @param arg Unused task argument
 */
static void rec_test_task(void *arg)
{
    record_wav(handles->i2s_rx_handle);
    task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Create and show recording progress message box
 *
 * Creates a popup showing recording progress and starts the recording task.
 */
static void lv_create_wifi_msgbox(void)
{
    rec_msg = lv_msgbox_create(NULL);
    lv_obj_set_style_clip_corner(rec_msg, true, 0);

    lv_msgbox_add_title(rec_msg, "rec test");

    /* setting fixed size */
    lv_obj_set_size(rec_msg, 200, 200);

    lv_obj_t * exit_but = lv_msgbox_add_close_button(rec_msg);

    /* setting's content*/
    lv_obj_t * content = lv_msgbox_get_content(rec_msg);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_right(content, -1, LV_PART_SCROLLBAR);

    msg_content_label = lv_label_create(content);
    lv_label_set_text(msg_content_label, "Recording");
    lv_obj_center(msg_content_label);
    xTaskCreate(rec_test_task, "rec_test_task", 1024 * 6, NULL, 4, &task_handle);
}

/**
 * @brief Start recording button event handler
 *
 * Opens the recording popup when the button is clicked.
 *
 * @param e LVGL event
 */
static void event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        lv_create_wifi_msgbox();
    }
}

/**
 * @brief Create the recording UI
 *
 * Creates a styled "Start Recording" button centered on screen.
 * Uses custom blue gradient style with press animation.
 */
void lv_example_rec(void)
{
    /*Init the style for the default state*/
    static lv_style_t style;
    lv_style_init(&style);

    lv_style_set_radius(&style, 3);

    lv_style_set_bg_opa(&style, LV_OPA_100);
    lv_style_set_bg_color(&style, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_grad_color(&style, lv_palette_darken(LV_PALETTE_BLUE, 2));
    lv_style_set_bg_grad_dir(&style, LV_GRAD_DIR_VER);

    lv_style_set_border_opa(&style, LV_OPA_40);
    lv_style_set_border_width(&style, 2);
    lv_style_set_border_color(&style, lv_palette_main(LV_PALETTE_GREY));

    lv_style_set_shadow_width(&style, 8);
    lv_style_set_shadow_color(&style, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_shadow_offset_y(&style, 8);

    lv_style_set_outline_opa(&style, LV_OPA_COVER);
    lv_style_set_outline_color(&style, lv_palette_main(LV_PALETTE_BLUE));

    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_pad_all(&style, 10);

    /*Init the pressed style*/
    static lv_style_t style_pr;
    lv_style_init(&style_pr);

    /*Add a large outline when pressed*/
    lv_style_set_outline_width(&style_pr, 30);
    lv_style_set_outline_opa(&style_pr, LV_OPA_TRANSP);

    lv_style_set_translate_y(&style_pr, 5);
    lv_style_set_shadow_offset_y(&style_pr, 3);
    lv_style_set_bg_color(&style_pr, lv_palette_darken(LV_PALETTE_BLUE, 2));
    lv_style_set_bg_grad_color(&style_pr, lv_palette_darken(LV_PALETTE_BLUE, 4));

    /*Add a transition to the outline*/
    static lv_style_transition_dsc_t trans;
    static lv_style_prop_t props[] = {LV_STYLE_OUTLINE_WIDTH, LV_STYLE_OUTLINE_OPA, 0};
    lv_style_transition_dsc_init(&trans, props, lv_anim_path_linear, 300, 0, NULL);

    lv_style_set_transition(&style_pr, &trans);

    btn1 = lv_button_create(lv_screen_active());
    lv_obj_remove_style_all(btn1);                          /*Remove the style coming from the theme*/
    lv_obj_add_style(btn1, &style, 0);
    lv_obj_add_style(btn1, &style_pr, LV_STATE_PRESSED);
    lv_obj_set_size(btn1, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(btn1);
    lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);

    lv_obj_t * label = lv_label_create(btn1);
    lv_label_set_text(label, "start rec");
    lv_obj_center(label);
}

/*===========================================================================
 * App Lifecycle Methods
 *===========================================================================*/

/**
 * @brief Called when the recorder app is launched
 *
 * Initializes audio playback system (for playback after recording)
 * and creates the recording button UI.
 *
 * @return true on success
 */
bool PhoneRecConf::run(void)
{
    ESP_BROOKESIA_LOGD("Run");

    /* Initialize audio for playback after recording */
    Audio_Play_Init();

    /* Get BSP handles for I2S access */
    handles = bsp_display_get_handles();

    /* Create recording button UI */
    lv_example_rec();

    return true;
}

/**
 * @brief Handle back button press
 *
 * Closes the app and returns to home screen.
 *
 * @return true on success
 */
bool PhoneRecConf::back(void)
{
    ESP_BROOKESIA_LOGD("Back");

    /* Notify core to close the app */
    ESP_BROOKESIA_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

/**
 * @brief Called when the app is closed
 *
 * Cleans up event handlers, stops any running recording task,
 * and deinitializes audio resources.
 *
 * @return true on success
 */
bool PhoneRecConf::close(void)
{
    ESP_BROOKESIA_LOGD("Close");

    /* Remove button event callback */
    lv_obj_remove_event_cb(btn1, event_handler);
    btn1 = NULL;

    /* Stop recording task if still running */
    if (task_handle != NULL) {
        vTaskDelete(task_handle);
        task_handle = NULL;
    }

    /* Notify core and cleanup audio */
    ESP_BROOKESIA_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
    Audio_Play_Deinit();
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
