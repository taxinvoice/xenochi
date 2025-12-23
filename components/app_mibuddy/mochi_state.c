/**
 * @file mochi_state.c
 * @brief MochiState System - Core state machine implementation
 */

#include "mochi_state.h"
#include "mochi_assets.h"
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
 * State Configuration - Single Source of Truth
 *===========================================================================
 *
 * Each state's complete configuration is defined here:
 * - foreground.face: Face parameters (eyes, mouth, particles)
 * - foreground.sprite: Overlay image
 * - background.image: Background image
 * - audio.enter: Sound on state entry
 * - audio.loop: Looping sound while in state
 *
 * To customize a state, modify its entry here or call mochi_configure_state().
 */

#define MOCHI_FACE_INIT(eye_sc, eye_ox, eye_oy, pupil_sz, eye_sq, \
                        m_type, m_open, f_sq, f_oy, f_rot, \
                        blush, sparkle, particle) \
    { \
        .eye_scale = eye_sc, .eye_offset_x = eye_ox, .eye_offset_y = eye_oy, \
        .pupil_size = pupil_sz, .eye_squish = eye_sq, \
        .mouth_type = m_type, .mouth_open = m_open, \
        .face_squish = f_sq, .face_offset_y = f_oy, .face_rotation = f_rot, \
        .show_blush = blush, .show_sparkles = sparkle, \
        .particle_type = particle \
    }

static mochi_state_config_t s_state_configs[MOCHI_STATE_MAX] = {
    /* MOCHI_STATE_HAPPY */
    {
        .foreground.face = MOCHI_FACE_INIT(
            1.0f, 0.0f, 0.0f,   /* eye: scale, offset_x, offset_y */
            1.0f, 0.0f,          /* pupil_size, eye_squish */
            MOCHI_MOUTH_SMILE, 0.3f,  /* mouth_type, mouth_open */
            0.0f, 0.0f, 0.0f,    /* face_squish, face_offset_y, face_rotation */
            true, true,          /* show_blush, show_sparkles */
            MOCHI_PARTICLE_FLOAT /* particle_type */
        ),
        /* Assets configured at runtime via mochi_configure_state() */
    },
    /* MOCHI_STATE_EXCITED */
    {
        .foreground.face = MOCHI_FACE_INIT(
            0.8f, 0.0f, 3.0f,
            0.7f, 0.3f,
            MOCHI_MOUTH_OPEN_SMILE, 0.7f,
            0.05f, 5.0f, 0.0f,
            true, true,
            MOCHI_PARTICLE_BURST
        ),
    },
    /* MOCHI_STATE_WORRIED */
    {
        .foreground.face = MOCHI_FACE_INIT(
            1.2f, 0.0f, -5.0f,
            1.3f, -0.1f,
            MOCHI_MOUTH_SMALL_O, 0.5f,
            -0.03f, -5.0f, 0.0f,
            false, false,
            MOCHI_PARTICLE_SWEAT
        ),
    },
    /* MOCHI_STATE_COOL */
    {
        .foreground.face = MOCHI_FACE_INIT(
            0.9f, 0.0f, 0.0f,
            0.9f, 0.15f,
            MOCHI_MOUTH_SMIRK, 0.2f,
            0.0f, 0.0f, 0.0f,
            false, true,
            MOCHI_PARTICLE_SPARKLE
        ),
    },
    /* MOCHI_STATE_DIZZY */
    {
        .foreground.face = MOCHI_FACE_INIT(
            1.0f, 0.0f, 0.0f,
            0.8f, 0.0f,
            MOCHI_MOUTH_WAVY, 0.4f,
            0.0f, 0.0f, 0.0f,
            false, false,
            MOCHI_PARTICLE_SPIRAL
        ),
    },
    /* MOCHI_STATE_PANIC */
    {
        .foreground.face = MOCHI_FACE_INIT(
            1.4f, 0.0f, 0.0f,
            0.4f, -0.2f,
            MOCHI_MOUTH_SCREAM, 1.0f,
            0.0f, 0.0f, 0.0f,
            false, false,
            MOCHI_PARTICLE_SWEAT
        ),
    },
    /* MOCHI_STATE_SLEEPY */
    {
        .foreground.face = MOCHI_FACE_INIT(
            0.15f, 0.0f, 8.0f,
            0.5f, 0.8f,
            MOCHI_MOUTH_SMILE, 0.2f,
            0.0f, 3.0f, -3.0f,
            true, false,
            MOCHI_PARTICLE_ZZZ
        ),
        /* Assets configured at runtime via mochi_configure_state() */
    },
    /* MOCHI_STATE_SHOCKED */
    {
        .foreground.face = MOCHI_FACE_INIT(
            1.3f, 0.0f, 0.0f,
            0.3f, -0.2f,
            MOCHI_MOUTH_SMALL_O, 0.8f,
            0.0f, 0.0f, 0.0f,
            false, false,
            MOCHI_PARTICLE_NONE
        ),
    },
};

/*===========================================================================
 * Asset Setup Callback
 *===========================================================================*/

static mochi_asset_setup_fn s_asset_setup_callback = NULL;

void mochi_register_asset_setup(mochi_asset_setup_fn setup_fn) {
    s_asset_setup_callback = setup_fn;
    ESP_LOGI(TAG, "Asset setup callback %s", setup_fn ? "registered" : "cleared");
}

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

    /* Asset overlay objects */
    lv_obj_t *sprite_obj;       /* Sprite overlay image */
    lv_obj_t *background_obj;   /* Background image */
} s_mochi = {
    .initialized = false,
    .created = false,
    .paused = false,
    .current_state = MOCHI_STATE_HAPPY,
    .current_activity = MOCHI_ACTIVITY_IDLE,
    .current_theme = MOCHI_THEME_SAKURA,
    .intensity = 0.7f,
    .sprite_obj = NULL,
    .background_obj = NULL,
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

/* Unused for now - can be used if needed for detailed logging
static const char* const ASSET_SOURCE_NAMES[] = {
    "None",
    "Embedded",
    "SD Card",
};
*/

/*===========================================================================
 * Asset Logging Helpers
 *===========================================================================*/

static void log_sound_asset(const char *label, const mochi_sound_asset_t *asset) {
    if (!asset || asset->source == MOCHI_ASSET_NONE) {
        ESP_LOGI(TAG, "  %s: None", label);
        return;
    }

    if (asset->source == MOCHI_ASSET_EMBEDDED) {
        ESP_LOGI(TAG, "  %s: Embedded PCM (%zu samples @ %lu Hz, %d ch)",
                 label,
                 asset->embedded.pcm_len,
                 (unsigned long)asset->embedded.sample_rate,
                 asset->embedded.channels);
    } else if (asset->source == MOCHI_ASSET_SDCARD) {
        ESP_LOGI(TAG, "  %s: SD Card [%s]", label, asset->sd_path ? asset->sd_path : "null");
    }
}

static void log_image_asset(const char *label, const mochi_image_asset_t *asset) {
    if (!asset || asset->source == MOCHI_ASSET_NONE) {
        ESP_LOGI(TAG, "  %s: None", label);
        return;
    }

    if (asset->source == MOCHI_ASSET_EMBEDDED) {
        ESP_LOGI(TAG, "  %s: Embedded image (%p)", label, (void*)asset->embedded);
    } else if (asset->source == MOCHI_ASSET_SDCARD) {
        ESP_LOGI(TAG, "  %s: SD Card [%s]", label, asset->sd_path ? asset->sd_path : "null");
    }
}

/*===========================================================================
 * Internal Functions
 *===========================================================================*/

/**
 * @brief Get face parameters for state, modified by intensity
 */
static void get_state_params(mochi_state_t state, float intensity, mochi_face_params_t *out) {
    if (state >= MOCHI_STATE_MAX) {
        state = MOCHI_STATE_HAPPY;
    }

    /* Copy face params from config */
    memcpy(out, &s_state_configs[state].foreground.face, sizeof(mochi_face_params_t));

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
 * @brief Apply asset configuration for state
 */
static void apply_state_assets(mochi_state_t state) {
    const mochi_state_config_t *cfg = &s_state_configs[state];

    /* Log all configured assets for this state */
    ESP_LOGI(TAG, "=== Assets for %s state ===", STATE_NAMES[state]);
    log_image_asset("Background", &cfg->background.image);
    log_image_asset("Sprite", &cfg->foreground.sprite);
    log_sound_asset("Enter Sound", &cfg->audio.enter);
    log_sound_asset("Loop Sound", &cfg->audio.loop);

    /* Play enter sound if configured */
    if (cfg->audio.enter.source != MOCHI_ASSET_NONE) {
        ESP_LOGI(TAG, "Playing enter sound...");
        esp_err_t err = mochi_play_asset_sound(&cfg->audio.enter, false);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to play enter sound: %s", esp_err_to_name(err));
        }
    }

    /* Update sprite overlay */
    if (cfg->foreground.sprite.source != MOCHI_ASSET_NONE) {
        if (s_mochi.sprite_obj == NULL) {
            /* Create sprite object */
            s_mochi.sprite_obj = mochi_create_asset_image(s_mochi.parent, &cfg->foreground.sprite);
            if (s_mochi.sprite_obj) {
                lv_obj_center(s_mochi.sprite_obj);
                lv_obj_set_pos(s_mochi.sprite_obj,
                               lv_obj_get_x(s_mochi.sprite_obj) + cfg->foreground.sprite_x,
                               lv_obj_get_y(s_mochi.sprite_obj) + cfg->foreground.sprite_y);
            }
        } else {
            mochi_update_asset_image(s_mochi.sprite_obj, &cfg->foreground.sprite);
            lv_obj_clear_flag(s_mochi.sprite_obj, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (s_mochi.sprite_obj) {
        lv_obj_add_flag(s_mochi.sprite_obj, LV_OBJ_FLAG_HIDDEN);
    }

    /* Update background */
    if (cfg->background.image.source != MOCHI_ASSET_NONE) {
        if (s_mochi.background_obj == NULL) {
            s_mochi.background_obj = mochi_create_asset_image(s_mochi.parent, &cfg->background.image);
            if (s_mochi.background_obj) {
                lv_obj_move_to_index(s_mochi.background_obj, 0);  /* Send to back */
                lv_obj_center(s_mochi.background_obj);
            }
        } else {
            mochi_update_asset_image(s_mochi.background_obj, &cfg->background.image);
            lv_obj_clear_flag(s_mochi.background_obj, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (s_mochi.background_obj) {
        lv_obj_add_flag(s_mochi.background_obj, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief Apply current state to rendering
 */
static void apply_state(void) {
    if (!s_mochi.created) return;

    const mochi_theme_t *theme = mochi_get_theme_colors(s_mochi.current_theme);

    /* Get face params from config, with intensity applied */
    get_state_params(s_mochi.current_state, s_mochi.intensity, &s_mochi.current_params);

    /* Update face rendering */
    mochi_face_update(&s_mochi.current_params, theme);

    /* Update particles */
    mochi_particles_set_type(s_mochi.current_params.particle_type, theme);

    /* Start appropriate animation */
    mochi_anim_start(s_mochi.current_state, s_mochi.current_activity);

    /* Apply asset configuration (sounds, sprites, background) */
    apply_state_assets(s_mochi.current_state);

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

    /* Call asset setup callback if registered */
    if (s_asset_setup_callback) {
        ESP_LOGI(TAG, "Calling asset setup callback");
        s_asset_setup_callback();
    }

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
 * Public API - Asset Configuration
 *===========================================================================*/

esp_err_t mochi_configure_state(mochi_state_t state, const mochi_state_config_t *config) {
    if (state >= MOCHI_STATE_MAX) {
        ESP_LOGE(TAG, "Invalid state for config: %d", state);
        return ESP_ERR_INVALID_ARG;
    }

    if (config == NULL) {
        /* Clear config for this state */
        memset(&s_state_configs[state], 0, sizeof(mochi_state_config_t));
        ESP_LOGI(TAG, "Cleared asset config for state: %s", STATE_NAMES[state]);
    } else {
        /* Copy config */
        memcpy(&s_state_configs[state], config, sizeof(mochi_state_config_t));
        ESP_LOGI(TAG, "Set asset config for state: %s", STATE_NAMES[state]);
    }

    return ESP_OK;
}

const mochi_state_config_t* mochi_get_state_config(mochi_state_t state) {
    if (state >= MOCHI_STATE_MAX) {
        return NULL;
    }
    return &s_state_configs[state];
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
        return &s_state_configs[MOCHI_STATE_HAPPY].foreground.face;
    }
    return &s_state_configs[s_mochi.current_state].foreground.face;
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
