/**
 * @file mochi_sounds.h
 * @brief Embedded sounds for MochiState
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 440Hz beep tone - 8kHz mono 16-bit PCM (~100ms)
 * 800 samples = 100ms at 8kHz
 */
extern const int16_t mochi_beep_8k[];

#define MOCHI_BEEP_8K_LEN      800
#define MOCHI_BEEP_8K_RATE     8000
#define MOCHI_BEEP_8K_CHANNELS 1

#ifdef __cplusplus
}
#endif
