/**
 * @file sdcard_player.h
 * @brief SD Card MP3 Player module using ESP-ADF
 *
 * This module provides SD card initialization and MP3 playback functionality
 * using the ESP-ADF audio pipeline.
 */

#ifndef SDCARD_PLAYER_H
#define SDCARD_PLAYER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// SD Card SPI pins for Waveshare ESP32-C6-Touch-LCD-1.83
#define SDCARD_SPI_HOST     SPI2_HOST
#define SDCARD_PIN_CS       17
#define SDCARD_PIN_SCLK     1
#define SDCARD_PIN_MISO     16
#define SDCARD_PIN_MOSI     2
#define SDCARD_MOUNT_POINT  "/sdcard"

/**
 * @brief Initialize SD card and audio subsystem
 *
 * Mounts the SD card and initializes the audio codec and pipeline.
 * Must be called before any playback functions.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sdcard_player_init(void);

/**
 * @brief Deinitialize SD card and audio subsystem
 *
 * Stops any playing audio, unmounts SD card, and releases resources.
 */
void sdcard_player_deinit(void);

/**
 * @brief Check if SD card player is initialized
 *
 * @return true if initialized, false otherwise
 */
bool sdcard_player_is_initialized(void);

/**
 * @brief Play an MP3 file from SD card
 *
 * Plays the specified MP3 file. Blocks until playback completes or
 * an error occurs if wait_for_end is true.
 *
 * @param filepath Path to the MP3 file (e.g., "/sdcard/sounds/startup.mp3")
 * @param wait_for_end If true, blocks until playback finishes
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sdcard_player_play(const char *filepath, bool wait_for_end);

/**
 * @brief Play startup sound from SD card
 *
 * Convenience function to play /sdcard/sounds/startup.mp3
 * Blocks until playback completes.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sdcard_player_play_startup(void);

/**
 * @brief Stop current playback
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sdcard_player_stop(void);

/**
 * @brief Check if audio is currently playing
 *
 * @return true if playing, false otherwise
 */
bool sdcard_player_is_playing(void);

#ifdef __cplusplus
}
#endif

#endif // SDCARD_PLAYER_H
