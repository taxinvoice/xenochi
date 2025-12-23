/**
 * @file mochi_audio.c
 * @brief MochiState Audio Integration - Sound playback for states
 *
 * Integrates with the existing audio_play component for:
 * - One-shot sounds on state changes
 * - Looping background audio
 */

#include "mochi_state.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "mochi_audio";

/*===========================================================================
 * External Audio API (from audio_play component)
 *===========================================================================*/

/* These functions are defined in audio_driver.c */
extern void Audio_Play_Music(const char *url);
extern void Audio_Stop_Play(void);
extern void Audio_Pause_Play(void);
extern void Audio_Resume_Play(void);

/* Audio state check - may not be available, so we'll define our own tracking */
/* extern esp_asp_state_t Audio_Get_Current_State(void); */

/*===========================================================================
 * Static Variables
 *===========================================================================*/

static struct {
    bool playing;
    bool loop_mode;
    char current_path[128];
    lv_timer_t *loop_timer;
} s_audio = {
    .playing = false,
    .loop_mode = false,
    .current_path = {0},
    .loop_timer = NULL,
};

/*===========================================================================
 * Loop Monitor Timer
 *===========================================================================*/

/**
 * @brief Timer callback to check if looping audio needs restart
 *
 * Since we don't have direct access to audio completion events,
 * we use a timer to periodically check and restart if needed.
 * This is a simplified approach - in production you'd want
 * proper event callbacks from the audio system.
 */
static void loop_timer_cb(lv_timer_t *timer) {
    if (!s_audio.playing || !s_audio.loop_mode) {
        return;
    }

    /* For now, we rely on the audio system to handle looping
     * or we'd need to integrate with Audio_Get_Current_State()
     * This timer is a placeholder for future loop detection */
}

/*===========================================================================
 * Forward Declarations
 *===========================================================================*/

void mochi_audio_stop(void);

/*===========================================================================
 * Public Functions
 *===========================================================================*/

void mochi_audio_play(const char *path, bool loop) {
    if (path == NULL || path[0] == '\0') {
        ESP_LOGW(TAG, "Empty audio path");
        return;
    }

    /* Stop any current playback */
    if (s_audio.playing) {
        mochi_audio_stop();
    }

    /* Build URL for audio driver */
    char audio_url[140];
    if (strncmp(path, "file://", 7) == 0) {
        /* Already a URL */
        strncpy(audio_url, path, sizeof(audio_url) - 1);
    } else if (path[0] == '/') {
        /* Absolute path - convert to file URL */
        snprintf(audio_url, sizeof(audio_url), "file://%s", path);
    } else {
        /* Relative path - assume /sdcard/ */
        snprintf(audio_url, sizeof(audio_url), "file:///sdcard/%s", path);
    }
    audio_url[sizeof(audio_url) - 1] = '\0';

    ESP_LOGI(TAG, "Playing audio: %s (loop=%d)", audio_url, loop);

    /* Store state */
    strncpy(s_audio.current_path, audio_url, sizeof(s_audio.current_path) - 1);
    s_audio.loop_mode = loop;
    s_audio.playing = true;

    /* Start playback using existing audio driver */
    Audio_Play_Music(audio_url);

    /* Start loop monitor if looping */
    if (loop && s_audio.loop_timer == NULL) {
        s_audio.loop_timer = lv_timer_create(loop_timer_cb, 500, NULL);
    }
}

void mochi_audio_stop(void) {
    if (!s_audio.playing) {
        return;
    }

    ESP_LOGI(TAG, "Stopping audio");

    /* Stop the audio driver */
    Audio_Stop_Play();

    /* Stop loop timer */
    if (s_audio.loop_timer != NULL) {
        lv_timer_delete(s_audio.loop_timer);
        s_audio.loop_timer = NULL;
    }

    s_audio.playing = false;
    s_audio.loop_mode = false;
    s_audio.current_path[0] = '\0';
}

/**
 * @brief Pause audio playback
 */
void mochi_audio_pause(void) {
    if (s_audio.playing) {
        Audio_Pause_Play();
    }
}

/**
 * @brief Resume audio playback
 */
void mochi_audio_resume(void) {
    if (s_audio.playing) {
        Audio_Resume_Play();
    }
}

/**
 * @brief Check if audio is currently playing
 */
bool mochi_audio_is_playing(void) {
    return s_audio.playing;
}
