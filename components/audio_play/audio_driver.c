/**
 * @file audio_driver.c
 * @brief Audio playback driver using ESP Audio Simple Player
 *
 * This driver provides audio playback functionality with a command queue pattern.
 * A dedicated FreeRTOS task processes playback commands (play, stop, pause, resume)
 * asynchronously, allowing the UI to remain responsive during audio operations.
 *
 * Architecture:
 * - Command Queue: Receives playback commands from other tasks
 * - Player Task: Processes commands and controls the audio pipeline
 * - Audio Pipeline: ESP Audio Simple Player handles decoding and output
 * - Power Amplifier: GPIO0 controls speaker amplifier enable
 *
 * Supported formats: WAV, MP3 (via ESP Audio Simple Player codecs)
 */

#include "audio_driver.h"
#include "string.h"
#include "errno.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "bsp_board.h"
#include "driver/gpio.h"

static const char *TAG = "audio play";

/*===========================================================================
 * Module State Variables
 *===========================================================================*/

static uint8_t Volume = CONFIG_AUDIO_DEFAULT_VOLUME;  /**< Current volume level (0-100), from Kconfig */
static esp_asp_handle_t handle = NULL;       /**< Audio Simple Player handle */
static TaskHandle_t xHandle;                 /**< Player task handle */
static FILE *audio_file = NULL;              /**< Current audio file handle for SPI-safe reading */

/**
 * @brief Player command types
 */
typedef enum {
    CMD_PLAY,       /**< Start playing a file */
    CMD_STOP,       /**< Stop playback */
    CMD_PAUSE,      /**< Pause playback */
    CMD_RESUME,     /**< Resume paused playback */
    CMD_DEINIT      /**< Deinitialize and exit task */
} player_cmd_t;

/**
 * @brief Player command message structure
 */
typedef struct {
    player_cmd_t cmd;   /**< Command type */
    char url[128];      /**< File URL (for CMD_PLAY), e.g., "file://sdcard/music.mp3" */
} player_queue_t;

#define QUEUE_LENGTH 5  /**< Maximum queued commands */

static QueueHandle_t cmd_queue = NULL;  /**< Command queue handle */
static bool audio_initialized = false;   /**< Initialization flag to prevent double init */

/*===========================================================================
 * Power Amplifier Control
 * GPIO0 enables/disables the speaker amplifier to save power and reduce noise
 *===========================================================================*/

/**
 * @brief Enable power amplifier (speaker on)
 */
static void Audio_PA_EN(void)
{
    gpio_set_level(GPIO_NUM_0, 1);
}

/**
 * @brief Disable power amplifier (speaker off)
 */
static void Audio_PA_DIS(void)
{
    gpio_set_level(GPIO_NUM_0, 0);
}




/*===========================================================================
 * Player Task
 * Main audio processing loop - runs in dedicated FreeRTOS task
 *===========================================================================*/

/**
 * @brief Audio player task function
 *
 * Runs continuously, waiting for commands from the queue.
 * Handles play/stop/pause/resume operations on the audio pipeline.
 *
 * @param pvParameters Unused task parameter
 */
void player_task(void *pvParameters)
{
    player_queue_t msg;

    while (1) {
        /* Block until a command is received */
        if (xQueueReceive(cmd_queue, &msg, portMAX_DELAY)) {
            switch (msg.cmd) {

                case CMD_PLAY: {
                    /* Start playing a new file */
                    ESP_LOGD(TAG, "Play: %s", msg.url);

                    if (handle == NULL) {
                        ESP_LOGD(TAG, "Audio player not initialized");
                        break;
                    }

                    Audio_PA_DIS();                              /* Disable amp during transition */

                    /* Only stop if currently playing to avoid NULL pipeline error */
                    esp_asp_state_t state;
                    if (esp_audio_simple_player_get_state(handle, &state) == ESP_GMF_ERR_OK) {
                        if (state == ESP_ASP_STATE_RUNNING || state == ESP_ASP_STATE_PAUSED) {
                            esp_audio_simple_player_stop(handle);
                        }
                    }

                    /* Close previous file if still open */
                    if (audio_file != NULL) {
                        fclose(audio_file);
                        audio_file = NULL;
                    }

                    /* Convert file:// URL to filesystem path and open file ourselves
                     * for SPI-synchronized reading (LCD and SD share SPI2 bus) */
                    const char *file_path = msg.url;
                    if (strncmp(file_path, "file://", 7) == 0) {
                        file_path = msg.url + 7;  /* Skip "file://" prefix -> "/sdcard/..." */
                    }

                    lvgl_port_lock(0);  /* Lock during file open (uses SD SPI) */
                    audio_file = fopen(file_path, "rb");
                    lvgl_port_unlock();

                    if (audio_file == NULL) {
                        /* Silently skip - file may not exist, this is expected */
                        ESP_LOGD(TAG, "Audio file not found: %s", file_path);
                        break;
                    }

                    /* URL still needed for decoder type detection (MP3/WAV based on extension) */
                    esp_audio_simple_player_run(handle, msg.url, NULL);
                    Audio_PA_EN();                               /* Enable amp for playback */
                    break;
                }

                case CMD_STOP: {
                    /* Stop playback completely */
                    ESP_LOGD(TAG, "Stop");

                    /* Only stop if currently playing to avoid NULL pipeline error */
                    if (handle != NULL) {
                        esp_asp_state_t state;
                        if (esp_audio_simple_player_get_state(handle, &state) == ESP_GMF_ERR_OK) {
                            if (state == ESP_ASP_STATE_RUNNING || state == ESP_ASP_STATE_PAUSED) {
                                esp_audio_simple_player_stop(handle);
                            }
                        }
                    }
                    /* Close audio file */
                    if (audio_file != NULL) {
                        fclose(audio_file);
                        audio_file = NULL;
                    }
                    Audio_PA_DIS();  /* Disable amp to save power */
                    break;
                }

                case CMD_PAUSE:
                    /* Pause current playback (can resume later) */
                    ESP_LOGD(TAG, "Pause");
                    Audio_PA_DIS();  /* Disable amp during pause */
                    if (handle != NULL) {
                        esp_audio_simple_player_pause(handle);
                    }
                    break;

                case CMD_RESUME:
                    /* Resume paused playback */
                    ESP_LOGD(TAG, "Resume");
                    if (handle != NULL) {
                        esp_audio_simple_player_resume(handle);
                    }
                    Audio_PA_EN();  /* Re-enable amp */
                    break;

                case CMD_DEINIT:
                    /* Cleanup and exit task */
                    gpio_set_level(GPIO_NUM_0, 0);
                    ESP_ERROR_CHECK(gpio_reset_pin(GPIO_NUM_0));
                    /* Close audio file if open */
                    if (audio_file != NULL) {
                        fclose(audio_file);
                        audio_file = NULL;
                    }
                    if (handle != NULL) {
                        esp_audio_simple_player_destroy(handle);
                        handle = NULL;
                    }
                    vQueueDelete(cmd_queue);
                    cmd_queue = NULL;
                    audio_initialized = false;
                    vTaskDelete(NULL);  /* Delete this task */
                    break;

                default:
                    ESP_LOGD(TAG, "Unknown command");
                    break;
            }
        }
    }
}



















/*===========================================================================
 * Audio Pipeline Callbacks
 *===========================================================================*/

/**
 * @brief Output data callback for audio pipeline
 *
 * Called by ESP Audio Simple Player when decoded audio data is ready.
 * Sends the audio samples to the I2S output.
 *
 * @param data Audio sample buffer
 * @param data_size Buffer size in bytes
 * @param ctx User context (unused)
 * @return 0 on success
 */
static int out_data_callback(uint8_t *data, int data_size, void *ctx)
{
    lvgl_port_lock(0);  /* Acquire LVGL mutex (coordinates with display updates) */
    esp_audio_play((int16_t*)data, data_size, 500 / portTICK_PERIOD_MS);
    lvgl_port_unlock();
    return 0;
}

/**
 * @brief Input data callback for audio pipeline (file reading)
 *
 * Called by ESP Audio Simple Player to read audio data from file.
 * Uses LVGL port lock to synchronize with LCD SPI operations since
 * both LCD and SD card share the same SPI2 bus.
 *
 * @param data Buffer to fill with audio data
 * @param data_size Number of bytes to read
 * @param ctx Unused (we use static audio_file)
 * @return Number of bytes read, 0 on EOF or error
 */
static int in_data_callback(uint8_t *data, int data_size, void *ctx)
{
    if (audio_file == NULL) {
        return 0;  /* No file open */
    }
    lvgl_port_lock(0);  /* Acquire LVGL mutex to prevent SPI bus conflict with LCD */
    int ret = fread(data, 1, data_size, audio_file);
    lvgl_port_unlock();
    ESP_LOGD(TAG, "%s-%d,rd size:%d", __func__, __LINE__, ret);
    return ret;
}

/**
 * @brief Audio pipeline event callback
 *
 * Handles events from the audio pipeline:
 * - MUSIC_INFO: Logs audio format information (sample rate, channels, etc.)
 * - STATE: Handles state changes (finished, error, etc.)
 *
 * @param event Event packet from pipeline
 * @param ctx User context (unused)
 * @return 0 on success
 */
static int mock_event_callback(esp_asp_event_pkt_t *event, void *ctx)
{
    if (event->type == ESP_ASP_EVENT_TYPE_MUSIC_INFO) {
        /* Audio format information received */
        esp_asp_music_info_t info = {0};
        memcpy(&info, event->payload, event->payload_size);
        ESP_LOGI(TAG, "Get info, rate:%d, channels:%d, bits:%d, bitrate=%d",
                 info.sample_rate, info.channels, info.bits, info.bitrate);
    }
    else if (event->type == ESP_ASP_EVENT_TYPE_STATE) {
        /* Playback state changed */
        esp_asp_state_t st = 0;
        memcpy(&st, event->payload, event->payload_size);

        ESP_LOGI(TAG, "Get State, %d,%s", st, esp_audio_simple_player_state_to_str(st));

        if (st == ESP_ASP_STATE_FINISHED) {
            /* Playback completed - close file and disable amplifier */
            ESP_LOGI(TAG, "Playback finished");
            if (audio_file != NULL) {
                fclose(audio_file);
                audio_file = NULL;
            }
            Audio_PA_DIS();
        }
    }
    return 0;
}

/*===========================================================================
 * Pipeline Management
 *===========================================================================*/

/**
 * @brief Initialize the audio pipeline
 *
 * Creates the ESP Audio Simple Player with output callback configured.
 * Must be called before any playback operations.
 */
static void pipeline_init(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    /* Configure audio pipeline with custom input callback for SPI-safe file reading.
     * Both LCD and SD card share SPI2 bus, so we handle file reading ourselves
     * with proper LVGL mutex synchronization to prevent bus conflicts. */
    esp_asp_cfg_t cfg = {
        .in.cb = in_data_callback,   /* Custom file reader with SPI synchronization */
        .in.user_ctx = NULL,
        .out.cb = out_data_callback, /* Output to I2S via BSP */
        .out.user_ctx = NULL,
    };

    /* Create player and register event callback */
    esp_gmf_err_t err = esp_audio_simple_player_new(&cfg, &handle);
    if (err != ESP_GMF_ERR_OK || handle == NULL) {
        ESP_LOGE(TAG, "Failed to create audio player: err=%d, handle=%p", err, (void*)handle);
        return;
    }
    ESP_LOGI(TAG, "Audio player created: handle=%p", (void*)handle);

    err = esp_audio_simple_player_set_event(handle, mock_event_callback, NULL);
    if (err != ESP_GMF_ERR_OK) {
        ESP_LOGW(TAG, "Failed to set event callback: %d", err);
    }
}

/**
 * @brief Deinitialize the audio pipeline
 *
 * Destroys the audio player and frees resources.
 */
static void pipeline_deinit(void)
{
    esp_audio_simple_player_destroy(handle);
}


/*===========================================================================
 * Public API - Playback Control
 * These functions send commands to the player task via queue
 *===========================================================================*/

/**
 * @brief Start playing an audio file
 *
 * Sends a play command to the player task. The file is played asynchronously.
 *
 * @param url File URL (e.g., "file://sdcard/music.mp3")
 * @return ESP_GMF_ERR_OK on success
 *
 * @example Audio_Play_Music("file://sdcard/song.wav");
 */
esp_gmf_err_t Audio_Play_Music(const char* url)
{
    if (cmd_queue == NULL || url == NULL) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    player_queue_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd = CMD_PLAY;
    memcpy(msg.url, url, strlen(url));
    xQueueSend(cmd_queue, &msg, portMAX_DELAY);
    return ESP_GMF_ERR_OK;
}

/**
 * @brief Stop audio playback
 *
 * Stops the current playback completely. Cannot be resumed.
 *
 * @return ESP_GMF_ERR_OK on success
 */
esp_gmf_err_t Audio_Stop_Play(void)
{
    if (cmd_queue == NULL) {
        return ESP_GMF_ERR_OK;  /* Not initialized yet, nothing to stop */
    }
    player_queue_t msg;
    msg.cmd = CMD_STOP;
    xQueueSend(cmd_queue, &msg, portMAX_DELAY);
    return ESP_GMF_ERR_OK;
}

/**
 * @brief Resume paused playback
 *
 * Resumes playback from where it was paused.
 *
 * @return ESP_GMF_ERR_OK on success
 */
esp_gmf_err_t Audio_Resume_Play(void)
{
    if (cmd_queue == NULL) {
        return ESP_GMF_ERR_OK;
    }
    player_queue_t msg;
    msg.cmd = CMD_RESUME;
    xQueueSend(cmd_queue, &msg, portMAX_DELAY);
    return ESP_GMF_ERR_OK;
}

/**
 * @brief Pause audio playback
 *
 * Pauses the current playback. Can be resumed with Audio_Resume_Play().
 *
 * @return ESP_GMF_ERR_OK on success
 */
esp_gmf_err_t Audio_Pause_Play(void)
{
    if (cmd_queue == NULL) {
        return ESP_GMF_ERR_OK;
    }
    player_queue_t msg;
    msg.cmd = CMD_PAUSE;
    xQueueSend(cmd_queue, &msg, portMAX_DELAY);
    return ESP_GMF_ERR_OK;
}

/**
 * @brief Get current playback state
 *
 * @return Current state (ESP_ASP_STATE_PLAYING, ESP_ASP_STATE_PAUSED, etc.)
 */
esp_asp_state_t Audio_Get_Current_State(void)
{
    if (handle == NULL) {
        return ESP_ASP_STATE_STOPPED;
    }
    esp_asp_state_t state;
    esp_gmf_err_t err = esp_audio_simple_player_get_state(handle, &state);
    if (err != ESP_GMF_ERR_OK) {
        ESP_LOGE("AUDIO", "Get state failed: %d", err);
        return ESP_ASP_STATE_ERROR;
    }
    return state;
}

/*===========================================================================
 * Public API - Initialization/Cleanup
 *===========================================================================*/

/**
 * @brief Initialize audio playback system
 *
 * Creates the command queue, configures GPIO for power amplifier,
 * initializes the audio pipeline, and starts the player task.
 *
 * Must be called before any playback operations.
 */
void Audio_Play_Init(void)
{
    /* Prevent double initialization */
    if (audio_initialized) {
        ESP_LOGI(TAG, "Audio already initialized, skipping");
        return;
    }

    /* Create command queue for player task */
    cmd_queue = xQueueCreate(QUEUE_LENGTH, sizeof(player_queue_t));
    if (cmd_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create command queue");
        return;
    }

    /* Configure GPIO0 as output for power amplifier control */
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << GPIO_NUM_0
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    /* Start with amplifier disabled */
    gpio_set_level(GPIO_NUM_0, 0);

    /* Initialize audio pipeline and start player task */
    pipeline_init();
    xTaskCreate(player_task, "player_task", 2048, NULL, 5, &xHandle);

    audio_initialized = true;

    /* Set initial volume from Kconfig default */
    Volume_Adjustment(CONFIG_AUDIO_DEFAULT_VOLUME);
    ESP_LOGI(TAG, "Audio playback system initialized, volume=%d", CONFIG_AUDIO_DEFAULT_VOLUME);
}

/**
 * @brief Deinitialize audio playback system
 *
 * Sends deinit command to player task, which will clean up
 * resources and delete itself.
 */
void Audio_Play_Deinit(void)
{
    player_queue_t msg;
    msg.cmd = CMD_DEINIT;
    xQueueSend(cmd_queue, &msg, portMAX_DELAY);
}

/*===========================================================================
 * Public API - Volume Control
 *===========================================================================*/

/**
 * @brief Set audio playback volume
 *
 * @param Vol Volume level (0-100, where Volume_MAX is the maximum)
 */
void Volume_Adjustment(uint8_t Vol)
{
    if (Vol > Volume_MAX) {
        printf("Audio: Volume value out of range. Please enter 0 to %d\r\n", Volume_MAX);
    }
    else {
        esp_audio_set_play_vol(Vol);
        Volume = Vol;
    }
}

/**
 * @brief Get current audio volume level
 *
 * @return Current volume (0-100)
 */
uint8_t get_audio_volume(void)
{
    return Volume;
}

/*===========================================================================
 * Embedded PCM Playback
 *===========================================================================*/

/** Target sample rate for codec */
#define TARGET_SAMPLE_RATE  44100

/** Chunk size for PCM output (in samples) */
#define PCM_CHUNK_SAMPLES   512

/**
 * @brief Play embedded PCM audio data directly
 *
 * Plays raw PCM samples from memory with sample rate conversion.
 * This function is synchronous - it blocks until playback is complete.
 *
 * @param pcm_data Pointer to 16-bit PCM sample array
 * @param samples Number of samples
 * @param sample_rate Source sample rate in Hz
 * @param channels Number of channels (1=mono, 2=stereo)
 * @param loop If true, loop continuously (NOT YET IMPLEMENTED)
 * @return ESP_GMF_ERR_OK on success
 */
esp_gmf_err_t Audio_Play_PCM(const int16_t *pcm_data, size_t samples,
                              uint32_t sample_rate, uint8_t channels, bool loop)
{
    if (!pcm_data || samples == 0) {
        ESP_LOGE(TAG, "Invalid PCM data");
        return ESP_GMF_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Playing embedded PCM: %zu samples @ %lu Hz, %d ch",
             samples, (unsigned long)sample_rate, channels);

    /* Reset logging flag to get detailed debug output */
    esp_audio_reset_log_flag();

    /* Prepare codec for direct PCM playback (re-open, set volume, unmute) */
    esp_err_t prep_ret = esp_audio_prepare_for_pcm();
    if (prep_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to prepare codec for PCM: %s", esp_err_to_name(prep_ret));
        return ESP_GMF_ERR_FAIL;
    }

    /* Restore user-set volume (esp_audio_prepare_for_pcm sets hardcoded PLAYER_VOLUME) */
    esp_audio_set_play_vol(Volume);
    ESP_LOGI(TAG, "Volume set to user level: %d", Volume);

    /* Enable power amplifier and wait for it to stabilize */
    Audio_PA_EN();
    ESP_LOGI(TAG, "PA enabled (GPIO0=1)");

    vTaskDelay(pdMS_TO_TICKS(20));  /* 20ms for amp and codec startup */

    /* Calculate resampling ratio */
    float ratio = (float)TARGET_SAMPLE_RATE / (float)sample_rate;
    size_t output_samples = (size_t)(samples * ratio);

    /* Allocate output buffer for stereo samples */
    int16_t *out_buf = malloc(PCM_CHUNK_SAMPLES * 2 * sizeof(int16_t));
    if (!out_buf) {
        ESP_LOGE(TAG, "Failed to allocate PCM buffer");
        Audio_PA_DIS();
        return ESP_GMF_ERR_MEMORY_LACK;
    }

    /* Process in chunks with linear interpolation resampling */
    size_t out_idx = 0;
    while (out_idx < output_samples) {
        size_t chunk_size = PCM_CHUNK_SAMPLES;
        if (out_idx + chunk_size > output_samples) {
            chunk_size = output_samples - out_idx;
        }

        /* Fill output buffer with resampled stereo data */
        for (size_t i = 0; i < chunk_size; i++) {
            /* Calculate source sample position (fractional) */
            float src_pos = (float)(out_idx + i) / ratio;
            size_t src_idx = (size_t)src_pos;
            float frac = src_pos - (float)src_idx;

            /* Clamp to valid range */
            if (src_idx >= samples - 1) {
                src_idx = samples - 2;
                frac = 1.0f;
            }

            /* Linear interpolation */
            int16_t sample;
            if (channels == 1) {
                /* Mono input */
                int32_t s0 = pcm_data[src_idx];
                int32_t s1 = pcm_data[src_idx + 1];
                sample = (int16_t)(s0 + (int32_t)(frac * (float)(s1 - s0)));
            } else {
                /* Stereo input - just use left channel for now */
                int32_t s0 = pcm_data[src_idx * 2];
                int32_t s1 = pcm_data[(src_idx + 1) * 2];
                sample = (int16_t)(s0 + (int32_t)(frac * (float)(s1 - s0)));
            }

            /* Output as stereo (duplicate to both channels) */
            out_buf[i * 2] = sample;      /* Left */
            out_buf[i * 2 + 1] = sample;  /* Right */
        }

        /* Send to codec */
        esp_err_t play_ret = esp_audio_play(out_buf, chunk_size * 2 * sizeof(int16_t), 100 / portTICK_PERIOD_MS);
        if (out_idx == 0) {
            /* Log first chunk details */
            ESP_LOGI(TAG, "First chunk: samples[0-3]=%d,%d,%d,%d ret=%d",
                     out_buf[0], out_buf[1], out_buf[2], out_buf[3], play_ret);
        }
        out_idx += chunk_size;
    }

    /* Send silence to flush the DMA buffer and ensure all audio reaches the speaker */
    memset(out_buf, 0, PCM_CHUNK_SAMPLES * 2 * sizeof(int16_t));
    esp_audio_play(out_buf, PCM_CHUNK_SAMPLES * 2 * sizeof(int16_t), 100 / portTICK_PERIOD_MS);
    esp_audio_play(out_buf, PCM_CHUNK_SAMPLES * 2 * sizeof(int16_t), 100 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Sent silence to flush DMA buffer");

    free(out_buf);

    /* Wait for I2S DMA buffer to finish outputting before disabling PA.
     * At 44.1kHz stereo 32-bit, the I2S buffer can hold a few ms of audio.
     * Add 100ms delay to ensure last samples reach the speaker. */
    ESP_LOGI(TAG, "Waiting for I2S DMA to finish...");
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Disable power amplifier after playback */
    Audio_PA_DIS();
    ESP_LOGI(TAG, "PA disabled (GPIO0=0)");

    ESP_LOGI(TAG, "Embedded PCM playback complete");
    return ESP_GMF_ERR_OK;
}