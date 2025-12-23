/**
 * @file mochi_state.c
 * @brief MochiState System - Core state machine implementation
 */

#include "mochi_state.h"
#include "mochi_theme.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char *TAG = "mochi";

/*===========================================================================
 * Forward Declarations (Internal Functions)
 *===========================================================================*/

/* From mochi_face.c */
extern void mochi_face_create(lv_obj_t *parent);
extern void mochi_face_destroy(void);
extern void mochi_face_update(const mochi_face_params_t *params, const mochi_theme_t *theme);
extern void mochi_face_set_visible(bool visible);

/* From mochi_anim.c */
extern void mochi_anim_init(void);
extern void mochi_anim_deinit(void);
extern void mochi_anim_start(mochi_state_t state, mochi_activity_t activity);
extern void mochi_anim_stop(void);
extern void mochi_anim_pause(void);
extern void mochi_anim_resume(void);
extern void mochi_anim_set_intensity(float intensity);

/* From mochi_particles.c */
extern void mochi_particles_create(lv_obj_t *parent);
extern void mochi_particles_destroy(void);
extern void mochi_particles_set_type(mochi_particle_type_t type, const mochi_theme_t *theme);
extern void mochi_particles_update(int frame);

/* From mochi_audio.c */
extern void mochi_audio_play(const char *path, bool loop);
extern void mochi_audio_stop(void);

/*===========================================================================
 * Default State Parameters
 *===========================================================================*/

/**
 * @brief Default face parameters for each state
 */
static const mochi_face_params_t STATE_DEFAULTS[MOCHI_STATE_MAX] = {
    /* MOCHI_STATE_HAPPY */
    {
        .eye_scale = 1.0f,
        .eye_offset_x = 0.0f,
        .eye_offset_y = 0.0f,
        .pupil_size = 1.0f,
        .eye_squish = 0.0f,
        .mouth_type = MOCHI_MOUTH_SMILE,
        .mouth_open = 0.3f,
        .face_squish = 0.0f,
        .face_offset_y = 0.0f,
        .face_rotation = 0.0f,
        .show_blush = true,
        .show_sparkles = true,
        .particle_type = MOCHI_PARTICLE_FLOAT,
    },
    /* MOCHI_STATE_EXCITED */
    {
        .eye_scale = 0.8f,
        .eye_offset_x = 0.0f,
        .eye_offset_y = 3.0f,
        .pupil_size = 0.7f,
        .eye_squish = 0.3f,
        .mouth_type = MOCHI_MOUTH_OPEN_SMILE,
        .mouth_open = 0.7f,
        .face_squish = 0.05f,
        .face_offset_y = 5.0f,
        .face_rotation = 0.0f,
        .show_blush = true,
        .show_sparkles = true,
        .particle_type = MOCHI_PARTICLE_BURST,
    },
    /* MOCHI_STATE_WORRIED */
    {
        .eye_scale = 1.2f,
        .eye_offset_x = 0.0f,
        .eye_offset_y = -5.0f,
        .pupil_size = 1.3f,
        .eye_squish = -0.1f,
        .mouth_type = MOCHI_MOUTH_SMALL_O,
        .mouth_open = 0.5f,
        .face_squish = -0.03f,
        .face_offset_y = -5.0f,
        .face_rotation = 0.0f,
        .show_blush = false,
        .show_sparkles = false,
        .particle_type = MOCHI_PARTICLE_SWEAT,
    },
    /* MOCHI_STATE_COOL */
    {
        .eye_scale = 0.9f,
        .eye_offset_x = 0.0f,
        .eye_offset_y = 0.0f,
        .pupil_size = 0.9f,
        .eye_squish = 0.15f,
        .mouth_type = MOCHI_MOUTH_SMIRK,
        .mouth_open = 0.2f,
        .face_squish = 0.0f,
        .face_offset_y = 0.0f,
        .face_rotation = 0.0f,
        .show_blush = false,
        .show_sparkles = true,
        .particle_type = MOCHI_PARTICLE_SPARKLE,
    },
    /* MOCHI_STATE_DIZZY */
    {
        .eye_scale = 1.0f,
        .eye_offset_x = 0.0f,
        .eye_offset_y = 0.0f,
        .pupil_size = 0.8f,
        .eye_squish = 0.0f,
        .mouth_type = MOCHI_MOUTH_WAVY,
        .mouth_open = 0.4f,
        .face_squish = 0.0f,
        .face_offset_y = 0.0f,
        .face_rotation = 0.0f,
        .show_blush = false,
        .show_sparkles = false,
        .particle_type = MOCHI_PARTICLE_SPIRAL,
    },
    /* MOCHI_STATE_PANIC */
    {
        .eye_scale = 1.4f,
        .eye_offset_x = 0.0f,
        .eye_offset_y = 0.0f,
        .pupil_size = 0.4f,
        .eye_squish = -0.2f,
        .mouth_type = MOCHI_MOUTH_SCREAM,
        .mouth_open = 1.0f,
        .face_squish = 0.0f,
        .face_offset_y = 0.0f,
        .face_rotation = 0.0f,
        .show_blush = false,
        .show_sparkles = false,
        .particle_type = MOCHI_PARTICLE_SWEAT,
    },
    /* MOCHI_STATE_SLEEPY */
    {
        .eye_scale = 0.15f,
        .eye_offset_x = 0.0f,
        .eye_offset_y = 8.0f,
        .pupil_size = 0.5f,
        .eye_squish = 0.8f,
        .mouth_type = MOCHI_MOUTH_SMILE,
        .mouth_open = 0.2f,
        .face_squish = 0.0f,
        .face_offset_y = 3.0f,
        .face_rotation = -3.0f,
        .show_blush = true,
        .show_sparkles = false,
        .particle_type = MOCHI_PARTICLE_ZZZ,
    },
    /* MOCHI_STATE_SHOCKED */
    {
        .eye_scale = 1.3f,
        .eye_offset_x = 0.0f,
        .eye_offset_y = 0.0f,
        .pupil_size = 0.3f,
        .eye_squish = -0.2f,
        .mouth_type = MOCHI_MOUTH_SMALL_O,
        .mouth_open = 0.8f,
        .face_squish = 0.0f,
        .face_offset_y = 0.0f,
        .face_rotation = 0.0f,
        .show_blush = false,
        .show_sparkles = false,
        .particle_type = MOCHI_PARTICLE_NONE,
    },
};

/*===========================================================================
 * Runtime State
 *===========================================================================*/

static struct {
    bool initialized;
    bool created;
    bool paused;

    mochi_state_t current_state;
    mochi_activity_t current_activity;
    mochi_theme_id_t current_theme;
    float intensity;

    mochi_face_params_t current_params;  /* Animated params */
    lv_obj_t *parent;
} s_mochi = {
    .initialized = false,
    .created = false,
    .paused = false,
    .current_state = MOCHI_STATE_HAPPY,
    .current_activity = MOCHI_ACTIVITY_IDLE,
    .current_theme = MOCHI_THEME_SAKURA,
    .intensity = 0.7f,
};

/*===========================================================================
 * State/Activity Name Tables
 *===========================================================================*/

static const char* const STATE_NAMES[MOCHI_STATE_MAX] = {
    "Happy",
    "Excited",
    "Worried",
    "Cool",
    "Dizzy",
    "Panic",
    "Sleepy",
    "Shocked",
};

static const char* const ACTIVITY_NAMES[MOCHI_ACTIVITY_MAX] = {
    "Idle",
    "Shake",
    "Bounce",
    "Spin",
    "Wiggle",
    "Nod",
    "Blink",
    "Snore",
    "Vibrate",
};

static const char* const THEME_NAMES[MOCHI_THEME_MAX] = {
    "Sakura",
    "Mint",
    "Lavender",
    "Peach",
    "Cloud",
};

/*===========================================================================
 * Internal Functions
 *===========================================================================*/

/**
 * @brief Get base parameters for state, modified by intensity
 */
static void get_state_params(mochi_state_t state, float intensity, mochi_face_params_t *out) {
    if (state >= MOCHI_STATE_MAX) {
        state = MOCHI_STATE_HAPPY;
    }

    /* Copy base params */
    memcpy(out, &STATE_DEFAULTS[state], sizeof(mochi_face_params_t));

    /* Apply intensity to animated values */
    out->eye_offset_x *= intensity;
    out->eye_offset_y *= intensity;
    out->face_squish *= intensity;
    out->face_offset_y *= intensity;
    out->face_rotation *= intensity;

    /* Scale eye_squish toward 0 based on intensity for excited/cool */
    if (state == MOCHI_STATE_EXCITED || state == MOCHI_STATE_COOL) {
        out->eye_squish *= intensity;
    }
}

/**
 * @brief Apply current state to rendering
 */
static void apply_state(void) {
    if (!s_mochi.created) return;

    const mochi_theme_t *theme = mochi_get_theme_colors(s_mochi.current_theme);

    /* Get base params for current state */
    get_state_params(s_mochi.current_state, s_mochi.intensity, &s_mochi.current_params);

    /* Update face rendering */
    mochi_face_update(&s_mochi.current_params, theme);

    /* Update particles */
    mochi_particles_set_type(s_mochi.current_params.particle_type, theme);

    /* Start appropriate animation */
    mochi_anim_start(s_mochi.current_state, s_mochi.current_activity);

    ESP_LOGI(TAG, "State: %s, Activity: %s, Theme: %s",
             STATE_NAMES[s_mochi.current_state],
             ACTIVITY_NAMES[s_mochi.current_activity],
             theme->name);
}

/*===========================================================================
 * Public API - Lifecycle
 *===========================================================================*/

esp_err_t mochi_init(void) {
    if (s_mochi.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing mochi state system");

    s_mochi.current_state = MOCHI_STATE_HAPPY;
    s_mochi.current_activity = MOCHI_ACTIVITY_IDLE;
    s_mochi.current_theme = MOCHI_THEME_SAKURA;
    s_mochi.intensity = 0.7f;
    s_mochi.paused = false;
    s_mochi.created = false;

    mochi_anim_init();

    s_mochi.initialized = true;
    return ESP_OK;
}

void mochi_deinit(void) {
    if (!s_mochi.initialized) return;

    ESP_LOGI(TAG, "Deinitializing mochi state system");

    if (s_mochi.created) {
        mochi_anim_stop();
        mochi_particles_destroy();
        mochi_face_destroy();
        s_mochi.created = false;
    }

    mochi_anim_deinit();
    s_mochi.initialized = false;
}

esp_err_t mochi_create(lv_obj_t *parent) {
    if (!s_mochi.initialized) {
        ESP_LOGE(TAG, "Not initialized, call mochi_init() first");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_mochi.created) {
        ESP_LOGW(TAG, "Already created");
        return ESP_OK;
    }

    if (parent == NULL) {
        ESP_LOGE(TAG, "Parent is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Creating mochi avatar");
    s_mochi.parent = parent;

    /* Create visual elements */
    mochi_face_create(parent);
    mochi_particles_create(parent);

    s_mochi.created = true;

    /* Apply initial state */
    apply_state();

    return ESP_OK;
}

/*===========================================================================
 * Public API - State Control
 *===========================================================================*/

esp_err_t mochi_set(mochi_state_t state, mochi_activity_t activity) {
    if (state >= MOCHI_STATE_MAX) {
        ESP_LOGE(TAG, "Invalid state: %d", state);
        return ESP_ERR_INVALID_ARG;
    }
    if (activity >= MOCHI_ACTIVITY_MAX) {
        ESP_LOGE(TAG, "Invalid activity: %d", activity);
        return ESP_ERR_INVALID_ARG;
    }

    s_mochi.current_state = state;
    s_mochi.current_activity = activity;

    if (s_mochi.created && !s_mochi.paused) {
        apply_state();
    }

    return ESP_OK;
}

esp_err_t mochi_set_state(mochi_state_t state) {
    return mochi_set(state, s_mochi.current_activity);
}

esp_err_t mochi_set_activity(mochi_activity_t activity) {
    return mochi_set(s_mochi.current_state, activity);
}

mochi_state_t mochi_get_state(void) {
    return s_mochi.current_state;
}

mochi_activity_t mochi_get_activity(void) {
    return s_mochi.current_activity;
}

const char* mochi_state_name(mochi_state_t state) {
    if (state >= MOCHI_STATE_MAX) return "Unknown";
    return STATE_NAMES[state];
}

const char* mochi_activity_name(mochi_activity_t activity) {
    if (activity >= MOCHI_ACTIVITY_MAX) return "Unknown";
    return ACTIVITY_NAMES[activity];
}

/*===========================================================================
 * Public API - Theme Control
 *===========================================================================*/

esp_err_t mochi_set_theme(mochi_theme_id_t theme) {
    if (theme >= MOCHI_THEME_MAX) {
        ESP_LOGE(TAG, "Invalid theme: %d", theme);
        return ESP_ERR_INVALID_ARG;
    }

    s_mochi.current_theme = theme;
    ESP_LOGI(TAG, "Theme set to: %s", THEME_NAMES[theme]);

    if (s_mochi.created && !s_mochi.paused) {
        apply_state();  /* Re-render with new theme */
    }

    return ESP_OK;
}

mochi_theme_id_t mochi_get_theme(void) {
    return s_mochi.current_theme;
}

void mochi_next_theme(void) {
    mochi_theme_id_t next = (s_mochi.current_theme + 1) % MOCHI_THEME_MAX;
    mochi_set_theme(next);
}

const char* mochi_theme_name(mochi_theme_id_t theme) {
    if (theme >= MOCHI_THEME_MAX) return "Unknown";
    return THEME_NAMES[theme];
}

/*===========================================================================
 * Public API - Animation Control
 *===========================================================================*/

void mochi_set_intensity(float intensity) {
    if (intensity < 0.2f) intensity = 0.2f;
    if (intensity > 1.0f) intensity = 1.0f;

    s_mochi.intensity = intensity;
    mochi_anim_set_intensity(intensity);

    if (s_mochi.created && !s_mochi.paused) {
        apply_state();
    }
}

float mochi_get_intensity(void) {
    return s_mochi.intensity;
}

/*===========================================================================
 * Public API - Lifecycle Hooks
 *===========================================================================*/

void mochi_pause(void) {
    if (s_mochi.paused) return;

    ESP_LOGI(TAG, "Pausing mochi");
    s_mochi.paused = true;
    mochi_anim_pause();
    mochi_face_set_visible(false);
}

void mochi_resume(void) {
    if (!s_mochi.paused) return;

    ESP_LOGI(TAG, "Resuming mochi");
    s_mochi.paused = false;
    mochi_face_set_visible(true);
    mochi_anim_resume();
    apply_state();
}

/*===========================================================================
 * Public API - Audio
 *===========================================================================*/

void mochi_play_sound(const char *path, bool loop) {
    if (path == NULL) return;
    mochi_audio_play(path, loop);
}

void mochi_stop_sound(void) {
    mochi_audio_stop();
}

/*===========================================================================
 * Accessors for Animation Module
 *===========================================================================*/

/**
 * @brief Get pointer to current animated params (for animation module to modify)
 */
mochi_face_params_t* mochi_get_current_params(void) {
    return &s_mochi.current_params;
}

/**
 * @brief Get base params for current state
 */
const mochi_face_params_t* mochi_get_base_params(void) {
    if (s_mochi.current_state >= MOCHI_STATE_MAX) {
        return &STATE_DEFAULTS[MOCHI_STATE_HAPPY];
    }
    return &STATE_DEFAULTS[s_mochi.current_state];
}

/**
 * @brief Get current theme colors
 */
const mochi_theme_t* mochi_get_current_theme(void) {
    return mochi_get_theme_colors(s_mochi.current_theme);
}

/**
 * @brief Trigger face redraw (called by animation module)
 */
void mochi_request_redraw(void) {
    if (!s_mochi.created || s_mochi.paused) return;
    mochi_face_update(&s_mochi.current_params, mochi_get_current_theme());
}
