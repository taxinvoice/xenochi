/**
 * @file sdcard_player.c
 * @brief SD Card MP3 Player implementation using ESP-ADF
 */

#include "sdcard_player.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"

#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "board.h"

static const char *TAG = "SDCARD_PLAYER";

// State
static bool s_initialized = false;
static bool s_playing = false;
static sdmmc_card_t *s_card = NULL;

// Audio pipeline handles
static audio_pipeline_handle_t s_pipeline = NULL;
static audio_element_handle_t s_fatfs_stream = NULL;
static audio_element_handle_t s_i2s_stream = NULL;
static audio_element_handle_t s_mp3_decoder = NULL;
static audio_event_iface_handle_t s_evt = NULL;
static esp_periph_set_handle_t s_periph_set = NULL;
static audio_board_handle_t s_board_handle = NULL;

/**
 * @brief Initialize SD card using SPI mode
 */
static esp_err_t sdcard_mount(void)
{
    ESP_LOGI(TAG, "Initializing SD card using SPI mode");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // Initialize SPI bus
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SDCARD_PIN_MOSI,
        .miso_io_num = SDCARD_PIN_MISO,
        .sclk_io_num = SDCARD_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t ret = spi_bus_initialize(SDCARD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        // Bus might already be initialized by display, try to continue
        ESP_LOGW(TAG, "SPI bus init returned %d, may already be initialized", ret);
    }

    // SD card SPI device configuration
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SDCARD_SPI_HOST;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SDCARD_PIN_CS;
    slot_config.host_id = SDCARD_SPI_HOST;

    ESP_LOGI(TAG, "Mounting SD card at %s", SDCARD_MOUNT_POINT);
    ret = esp_vfs_fat_sdspi_mount(SDCARD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "Make sure SD card is formatted with FAT32.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SD card (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    ESP_LOGI(TAG, "SD card mounted successfully");
    sdmmc_card_print_info(stdout, s_card);
    return ESP_OK;
}

/**
 * @brief Unmount SD card
 */
static void sdcard_unmount(void)
{
    if (s_card != NULL) {
        esp_vfs_fat_sdcard_unmount(SDCARD_MOUNT_POINT, s_card);
        ESP_LOGI(TAG, "SD card unmounted");
        s_card = NULL;
    }
}

/**
 * @brief Initialize audio pipeline
 */
static esp_err_t audio_pipeline_setup(void)
{
    ESP_LOGI(TAG, "Setting up audio pipeline");

    // Initialize peripherals management
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    s_periph_set = esp_periph_set_init(&periph_cfg);
    if (s_periph_set == NULL) {
        ESP_LOGE(TAG, "Failed to init periph set");
        return ESP_FAIL;
    }

    // Initialize audio board (codec)
    ESP_LOGI(TAG, "Initializing audio codec");
    s_board_handle = audio_board_init();
    if (s_board_handle == NULL) {
        ESP_LOGE(TAG, "Failed to init audio board");
        return ESP_FAIL;
    }
    audio_hal_ctrl_codec(s_board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    // Create audio pipeline
    ESP_LOGI(TAG, "Creating audio pipeline");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    s_pipeline = audio_pipeline_init(&pipeline_cfg);
    if (s_pipeline == NULL) {
        ESP_LOGE(TAG, "Failed to create audio pipeline");
        return ESP_FAIL;
    }

    // Create FATFS stream to read from SD card
    ESP_LOGI(TAG, "Creating FATFS stream reader");
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;
    s_fatfs_stream = fatfs_stream_init(&fatfs_cfg);
    if (s_fatfs_stream == NULL) {
        ESP_LOGE(TAG, "Failed to create FATFS stream");
        return ESP_FAIL;
    }

    // Create I2S stream to write to codec
    ESP_LOGI(TAG, "Creating I2S stream writer");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    s_i2s_stream = i2s_stream_init(&i2s_cfg);
    if (s_i2s_stream == NULL) {
        ESP_LOGE(TAG, "Failed to create I2S stream");
        return ESP_FAIL;
    }

    // Create MP3 decoder
    ESP_LOGI(TAG, "Creating MP3 decoder");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    s_mp3_decoder = mp3_decoder_init(&mp3_cfg);
    if (s_mp3_decoder == NULL) {
        ESP_LOGE(TAG, "Failed to create MP3 decoder");
        return ESP_FAIL;
    }

    // Register all elements to pipeline
    ESP_LOGI(TAG, "Registering pipeline elements");
    audio_pipeline_register(s_pipeline, s_fatfs_stream, "file");
    audio_pipeline_register(s_pipeline, s_mp3_decoder, "mp3");
    audio_pipeline_register(s_pipeline, s_i2s_stream, "i2s");

    // Link elements: [SD Card] --> fatfs --> mp3_decoder --> i2s --> [Codec]
    ESP_LOGI(TAG, "Linking pipeline: file-->mp3-->i2s");
    const char *link_tag[3] = {"file", "mp3", "i2s"};
    audio_pipeline_link(s_pipeline, &link_tag[0], 3);

    // Setup event listener
    ESP_LOGI(TAG, "Setting up event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    s_evt = audio_event_iface_init(&evt_cfg);
    if (s_evt == NULL) {
        ESP_LOGE(TAG, "Failed to create event interface");
        return ESP_FAIL;
    }

    audio_pipeline_set_listener(s_pipeline, s_evt);
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(s_periph_set), s_evt);

    ESP_LOGI(TAG, "Audio pipeline setup complete");
    return ESP_OK;
}

/**
 * @brief Cleanup audio pipeline
 */
static void audio_pipeline_cleanup(void)
{
    if (s_pipeline != NULL) {
        audio_pipeline_stop(s_pipeline);
        audio_pipeline_wait_for_stop(s_pipeline);
        audio_pipeline_terminate(s_pipeline);

        audio_pipeline_unregister(s_pipeline, s_fatfs_stream);
        audio_pipeline_unregister(s_pipeline, s_mp3_decoder);
        audio_pipeline_unregister(s_pipeline, s_i2s_stream);

        audio_pipeline_remove_listener(s_pipeline);
    }

    if (s_periph_set != NULL) {
        esp_periph_set_stop_all(s_periph_set);
        if (s_evt != NULL) {
            audio_event_iface_remove_listener(esp_periph_set_get_event_iface(s_periph_set), s_evt);
        }
    }

    if (s_evt != NULL) {
        audio_event_iface_destroy(s_evt);
        s_evt = NULL;
    }

    if (s_pipeline != NULL) {
        audio_pipeline_deinit(s_pipeline);
        s_pipeline = NULL;
    }

    if (s_fatfs_stream != NULL) {
        audio_element_deinit(s_fatfs_stream);
        s_fatfs_stream = NULL;
    }

    if (s_mp3_decoder != NULL) {
        audio_element_deinit(s_mp3_decoder);
        s_mp3_decoder = NULL;
    }

    if (s_i2s_stream != NULL) {
        audio_element_deinit(s_i2s_stream);
        s_i2s_stream = NULL;
    }

    if (s_periph_set != NULL) {
        esp_periph_set_destroy(s_periph_set);
        s_periph_set = NULL;
    }

    ESP_LOGI(TAG, "Audio pipeline cleaned up");
}

esp_err_t sdcard_player_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing SD card player");

    // Mount SD card
    esp_err_t ret = sdcard_mount();
    if (ret != ESP_OK) {
        return ret;
    }

    // Setup audio pipeline
    ret = audio_pipeline_setup();
    if (ret != ESP_OK) {
        sdcard_unmount();
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "SD card player initialized successfully");
    return ESP_OK;
}

void sdcard_player_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing SD card player");

    if (s_playing) {
        sdcard_player_stop();
    }

    audio_pipeline_cleanup();
    sdcard_unmount();

    s_initialized = false;
    ESP_LOGI(TAG, "SD card player deinitialized");
}

bool sdcard_player_is_initialized(void)
{
    return s_initialized;
}

esp_err_t sdcard_player_play(const char *filepath, bool wait_for_end)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (filepath == NULL) {
        ESP_LOGE(TAG, "Invalid filepath");
        return ESP_ERR_INVALID_ARG;
    }

    // Stop any current playback
    if (s_playing) {
        sdcard_player_stop();
    }

    ESP_LOGI(TAG, "Playing: %s", filepath);

    // Set the file URI
    audio_element_set_uri(s_fatfs_stream, filepath);

    // Reset pipeline state
    audio_pipeline_reset_ringbuffer(s_pipeline);
    audio_pipeline_reset_elements(s_pipeline);

    // Start playback
    esp_err_t ret = audio_pipeline_run(s_pipeline);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start pipeline: %s", esp_err_to_name(ret));
        return ret;
    }

    s_playing = true;

    if (wait_for_end) {
        // Wait for playback to complete
        while (s_playing) {
            audio_event_iface_msg_t msg;
            ret = audio_event_iface_listen(s_evt, &msg, pdMS_TO_TICKS(500));

            if (ret != ESP_OK) {
                continue;
            }

            // Handle music info from decoder
            if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
                msg.source == (void *)s_mp3_decoder &&
                msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {

                audio_element_info_t music_info = {0};
                audio_element_getinfo(s_mp3_decoder, &music_info);

                ESP_LOGI(TAG, "Music info: sample_rate=%d, bits=%d, channels=%d",
                         music_info.sample_rates, music_info.bits, music_info.channels);

                audio_element_setinfo(s_i2s_stream, &music_info);
                i2s_stream_set_clk(s_i2s_stream, music_info.sample_rates,
                                   music_info.bits, music_info.channels);
                continue;
            }

            // Check for stop/finish events
            if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
                msg.source == (void *)s_i2s_stream &&
                msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {

                int status = (int)msg.data;
                if (status == AEL_STATUS_STATE_STOPPED ||
                    status == AEL_STATUS_STATE_FINISHED) {
                    ESP_LOGI(TAG, "Playback finished");
                    s_playing = false;
                    break;
                }
            }
        }

        // Stop and reset pipeline
        audio_pipeline_stop(s_pipeline);
        audio_pipeline_wait_for_stop(s_pipeline);
    }

    return ESP_OK;
}

esp_err_t sdcard_player_play_startup(void)
{
    return sdcard_player_play("/sdcard/sounds/startup.mp3", true);
}

esp_err_t sdcard_player_stop(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_playing) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping playback");

    audio_pipeline_stop(s_pipeline);
    audio_pipeline_wait_for_stop(s_pipeline);

    s_playing = false;
    return ESP_OK;
}

bool sdcard_player_is_playing(void)
{
    return s_playing;
}
