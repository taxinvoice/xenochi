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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "bsp_board.h"
#include "driver/gpio.h"

static const char *TAG = "audio play";

/*===========================================================================
 * Module State Variables
 *===========================================================================*/

static uint8_t Volume = PLAYER_VOLUME;      /**< Current volume level (0-100) */
static esp_asp_handle_t handle = NULL;       /**< Audio Simple Player handle */
static TaskHandle_t xHandle;                 /**< Player task handle */

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

                case CMD_PLAY:
                    /* Start playing a new file */
                    printf("Command received: Play <%s>\n", msg.url);
                    Audio_PA_DIS();                              /* Disable amp during transition */
                    esp_audio_simple_player_stop(handle);        /* Stop any current playback */
                    esp_audio_simple_player_run(handle, msg.url, NULL);  /* Start new file */
                    Audio_PA_EN();                               /* Enable amp for playback */
                    break;

                case CMD_STOP:
                    /* Stop playback completely */
                    printf("Command received: Stop\n");
                    esp_audio_simple_player_stop(handle);
                    Audio_PA_DIS();  /* Disable amp to save power */
                    break;

                case CMD_PAUSE:
                    /* Pause current playback (can resume later) */
                    printf("Command received: Pause\n");
                    Audio_PA_DIS();  /* Disable amp during pause */
                    esp_audio_simple_player_pause(handle);
                    break;

                case CMD_RESUME:
                    /* Resume paused playback */
                    printf("Command received: Resume\n");
                    esp_audio_simple_player_resume(handle);
                    Audio_PA_EN();  /* Re-enable amp */
                    break;

                case CMD_DEINIT:
                    /* Cleanup and exit task */
                    gpio_set_level(GPIO_NUM_0, 0);
                    ESP_ERROR_CHECK(gpio_reset_pin(GPIO_NUM_0));
                    esp_audio_simple_player_destroy(handle);
                    vQueueDelete(cmd_queue);
                    cmd_queue = NULL;
                    vTaskDelete(NULL);  /* Delete this task */
                    break;

                default:
                    printf("Unknown command\n");
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
 *
 * @param data Buffer to fill with audio data
 * @param data_size Number of bytes to read
 * @param ctx File pointer (FILE*)
 * @return Number of bytes read
 */
static int in_data_callback(uint8_t *data, int data_size, void *ctx)
{
    int ret = fread(data, 1, data_size, ctx);
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
            /* Playback completed - disable amplifier */
            ESP_LOGI(TAG, "Playback finished");
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

    /* Configure audio pipeline with output callback */
    esp_asp_cfg_t cfg = {
        .in.cb = NULL,              /* Input from URL (file:// or http://) */
        .in.user_ctx = NULL,
        .out.cb = out_data_callback, /* Output to I2S via BSP */
        .out.user_ctx = NULL,
    };

    /* Create player and register event callback */
    esp_gmf_err_t err = esp_audio_simple_player_new(&cfg, &handle);
    err = esp_audio_simple_player_set_event(handle, mock_event_callback, NULL);
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
    esp_gmf_err_t err = ESP_GMF_ERR_OK;
    player_queue_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd = CMD_PLAY;
    memcpy(msg.url, url, strlen(url));
    xQueueSend(cmd_queue, &msg, portMAX_DELAY);
    return err;
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
    esp_gmf_err_t err = ESP_GMF_ERR_OK;
    player_queue_t msg;
    msg.cmd = CMD_STOP;
    xQueueSend(cmd_queue, &msg, portMAX_DELAY);
    return err;
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
    esp_gmf_err_t err = ESP_GMF_ERR_OK;
    player_queue_t msg;
    msg.cmd = CMD_RESUME;
    xQueueSend(cmd_queue, &msg, portMAX_DELAY);
    return err;
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
    esp_gmf_err_t err = ESP_GMF_ERR_OK;
    player_queue_t msg;
    msg.cmd = CMD_PAUSE;
    xQueueSend(cmd_queue, &msg, portMAX_DELAY);
    return err;
}

/**
 * @brief Get current playback state
 *
 * @return Current state (ESP_ASP_STATE_PLAYING, ESP_ASP_STATE_PAUSED, etc.)
 */
esp_asp_state_t Audio_Get_Current_State(void)
{
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
    /* Create command queue for player task */
    cmd_queue = xQueueCreate(QUEUE_LENGTH, sizeof(player_queue_t));
    if (cmd_queue == NULL) {
        printf("Failed to create command queue\n");
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