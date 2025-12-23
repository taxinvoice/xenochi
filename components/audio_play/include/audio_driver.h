#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_gmf_element.h"
#include "esp_gmf_pipeline.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_alc.h"
#include "esp_audio_simple_player.h"
#include "esp_audio_simple_player_advance.h"
#include "esp_codec_dev.h"
#include "esp_gmf_io.h"
#include "esp_gmf_io_embed_flash.h"
#include "esp_gmf_audio_dec.h"

#ifdef __cplusplus
extern "C" {
#endif

#define Volume_MAX  100

void Audio_Play_Init(void);
void Volume_Adjustment(uint8_t Vol);
uint8_t get_audio_volume(void);

esp_gmf_err_t Audio_Play_Music(const char* url);
esp_gmf_err_t Audio_Stop_Play(void);
esp_gmf_err_t Audio_Resume_Play(void);
esp_gmf_err_t Audio_Pause_Play(void);
esp_asp_state_t Audio_Get_Current_State(void);
void Audio_Play_Deinit(void);

/**
 * @brief Play embedded PCM audio data directly
 *
 * Plays raw PCM samples from memory. Handles sample rate conversion
 * to match the codec's native sample rate (44.1kHz).
 *
 * @param pcm_data Pointer to 16-bit PCM sample array
 * @param samples Number of samples (not bytes)
 * @param sample_rate Source sample rate in Hz (e.g., 8000, 22050, 44100)
 * @param channels Number of channels (1=mono, 2=stereo)
 * @param loop If true, loop the audio continuously
 * @return ESP_GMF_ERR_OK on success
 */
esp_gmf_err_t Audio_Play_PCM(const int16_t *pcm_data, size_t samples,
                              uint32_t sample_rate, uint8_t channels, bool loop);
#ifdef __cplusplus
}
#endif
