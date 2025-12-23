/**
 * @file mochi_anim.c
 * @brief MochiState Animation Controller - Handles all face animations
 *
 * Activity animations:
 * - Idle: Breathing (face squish oscillation)
 * - Shake: Rapid left-right movement
 * - Bounce: Up-down bouncing
 * - Spin: Slow rotation
 * - Wiggle: Side-to-side wobble
 * - Nod: Up-down nod
 * - Blink: Periodic eye blinks
 * - Snore: Breathing + zzz
 * - Vibrate: Fast micro-shake
 */

#include "mochi_state.h"
#include "mochi_theme.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "mochi_anim";

/*===========================================================================
 * Animation Parameters
 *===========================================================================*/

#define ANIM_TIMER_PERIOD_MS    25  /* 40 FPS */
#define PI                      3.14159265358979f

/* Animation frequencies in Hz */
#define IDLE_BREATH_FREQ        0.4f
#define SHAKE_FREQ              10.0f
#define BOUNCE_FREQ             3.0f
#define SPIN_FREQ               0.5f
#define WIGGLE_FREQ             4.0f
#define NOD_FREQ                2.0f
#define BLINK_INTERVAL_MS       3000
#define VIBRATE_FREQ            30.0f

/* Animation amplitudes */
#define IDLE_SQUISH_AMP         0.02f
#define IDLE_SWAY_AMP           2.0f
#define SHAKE_AMP               8.0f
#define BOUNCE_AMP_UP           5.0f
#define BOUNCE_AMP_DOWN         10.0f
#define WIGGLE_AMP              5.0f
#define NOD_AMP                 5.0f
#define VIBRATE_AMP             2.0f

/*===========================================================================
 * External Accessors (from mochi_state.c)
 *===========================================================================*/

extern mochi_face_params_t* mochi_get_current_params(void);
extern const mochi_face_params_t* mochi_get_base_params(void);
extern void mochi_request_redraw(void);

/*===========================================================================
 * Static Variables
 *===========================================================================*/

static struct {
    bool initialized;
    bool running;
    bool paused;

    mochi_state_t current_state;
    mochi_activity_t current_activity;
    float intensity;

    lv_timer_t *timer;
    uint32_t frame;           /* Animation frame counter */
    uint32_t last_blink_ms;   /* Last blink time */
    bool is_blinking;
    float blink_progress;     /* 0.0 to 1.0 */
} s_anim = {
    .initialized = false,
    .running = false,
    .paused = false,
    .intensity = 0.7f,
    .timer = NULL,
    .frame = 0,
    .last_blink_ms = 0,
    .is_blinking = false,
    .blink_progress = 0.0f,
};

/*===========================================================================
 * Animation Calculation Functions
 *===========================================================================*/

/**
 * @brief Apply idle breathing animation
 */
static void apply_idle_animation(mochi_face_params_t *params, float t) {
    /* Gentle breathing - face squish oscillation */
    params->face_squish = sinf(t * 2 * PI * IDLE_BREATH_FREQ) * IDLE_SQUISH_AMP * s_anim.intensity;

    /* Subtle eye wander */
    params->eye_offset_x = sinf(t * 0.5f) * IDLE_SWAY_AMP * s_anim.intensity;
    params->eye_offset_y = sinf(t * 0.3f) * (IDLE_SWAY_AMP * 0.5f) * s_anim.intensity;

    /* Subtle face sway */
    params->face_offset_y = sinf(t * 0.4f) * IDLE_SWAY_AMP * s_anim.intensity;
}

/**
 * @brief Apply shake animation
 */
static void apply_shake_animation(mochi_face_params_t *params, float t) {
    /* Rapid left-right shake */
    params->face_offset_y = sinf(t * 2 * PI * SHAKE_FREQ) * SHAKE_AMP * s_anim.intensity;

    /* Eyes move opposite to face */
    params->eye_offset_x = -sinf(t * 2 * PI * SHAKE_FREQ) * (SHAKE_AMP * 0.3f) * s_anim.intensity;
}

/**
 * @brief Apply bounce animation
 */
static void apply_bounce_animation(mochi_face_params_t *params, float t) {
    /* Bouncing motion */
    float bounce = sinf(t * 2 * PI * BOUNCE_FREQ);
    if (bounce > 0) {
        params->face_offset_y = -bounce * BOUNCE_AMP_UP * s_anim.intensity;
    } else {
        params->face_offset_y = -bounce * BOUNCE_AMP_DOWN * s_anim.intensity;
    }

    /* Squish when landing */
    params->face_squish = fabsf(bounce) * 0.05f * s_anim.intensity;
}

/**
 * @brief Apply spin animation
 */
static void apply_spin_animation(mochi_face_params_t *params, float t) {
    /* Slow rotation */
    params->face_rotation = fmodf(t * 360.0f * SPIN_FREQ, 360.0f) * s_anim.intensity;
}

/**
 * @brief Apply wiggle animation
 */
static void apply_wiggle_animation(mochi_face_params_t *params, float t) {
    /* Side-to-side wobble */
    params->face_rotation = sinf(t * 2 * PI * WIGGLE_FREQ) * WIGGLE_AMP * s_anim.intensity;
}

/**
 * @brief Apply nod animation
 */
static void apply_nod_animation(mochi_face_params_t *params, float t) {
    /* Up-down nod */
    params->face_offset_y = sinf(t * 2 * PI * NOD_FREQ) * NOD_AMP * s_anim.intensity;
}

/**
 * @brief Apply blink animation
 */
static void apply_blink_animation(mochi_face_params_t *params, uint32_t now_ms) {
    /* Check if time to blink */
    if (!s_anim.is_blinking && (now_ms - s_anim.last_blink_ms) >= BLINK_INTERVAL_MS) {
        s_anim.is_blinking = true;
        s_anim.blink_progress = 0.0f;
        s_anim.last_blink_ms = now_ms;
    }

    /* Animate blink */
    if (s_anim.is_blinking) {
        s_anim.blink_progress += 0.15f;  /* Speed of blink */

        float squish;
        if (s_anim.blink_progress < 0.5f) {
            /* Closing */
            squish = s_anim.blink_progress * 2.0f * 0.9f;
        } else if (s_anim.blink_progress < 1.0f) {
            /* Opening */
            squish = (1.0f - s_anim.blink_progress) * 2.0f * 0.9f;
        } else {
            /* Done */
            squish = 0.0f;
            s_anim.is_blinking = false;
        }

        params->eye_squish = squish;
    }
}

/**
 * @brief Apply snore animation (for sleepy state)
 */
static void apply_snore_animation(mochi_face_params_t *params, float t) {
    /* Slower, deeper breathing */
    params->face_squish = sinf(t * 2 * PI * 0.3f) * 0.03f * s_anim.intensity;
    params->face_offset_y = 3.0f + sinf(t * 2 * PI * 0.25f) * 2.0f * s_anim.intensity;
    params->face_rotation = -3.0f + sinf(t * 2 * PI * 0.2f) * 2.0f * s_anim.intensity;

    /* Mouth animation for snoring */
    params->mouth_open = 0.2f + sinf(t * 2 * PI * 0.4f) * 0.1f * s_anim.intensity;
}

/**
 * @brief Apply vibrate animation (for panic state)
 */
static void apply_vibrate_animation(mochi_face_params_t *params, float t) {
    /* Fast micro-shake */
    params->face_offset_y = sinf(t * 2 * PI * VIBRATE_FREQ) * VIBRATE_AMP * s_anim.intensity;
    params->eye_offset_x = cosf(t * 2 * PI * VIBRATE_FREQ * 1.3f) * VIBRATE_AMP * s_anim.intensity;
}

/**
 * @brief Apply state-specific animations (for Dizzy, Panic, etc.)
 */
static void apply_state_animation(mochi_face_params_t *params, float t) {
    switch (s_anim.current_state) {
        case MOCHI_STATE_DIZZY:
            /* Dizzy specific animations */
            params->eye_scale = 1.0f + sinf(t * 6.0f) * 0.15f * s_anim.intensity;
            params->eye_offset_x = sinf(t * 10.0f) * 6.0f * s_anim.intensity;
            params->eye_offset_y = cosf(t * 8.0f) * 4.0f * s_anim.intensity;
            params->face_rotation = sinf(t * 5.0f) * 5.0f * s_anim.intensity;
            params->face_offset_y = fabsf(sinf(t * 6.0f)) * 8.0f * s_anim.intensity;
            break;

        case MOCHI_STATE_PANIC:
            /* Panic rotation */
            params->face_rotation = fmodf(s_anim.frame * 8.0f * s_anim.intensity, 360.0f);
            break;

        case MOCHI_STATE_SLEEPY:
            /* Sleepy specific animations */
            params->eye_scale = 0.15f + sinf(t) * 0.05f;
            break;

        default:
            break;
    }
}

/*===========================================================================
 * Timer Callback
 *===========================================================================*/

static void anim_timer_cb(lv_timer_t *timer) {
    if (s_anim.paused) return;

    s_anim.frame++;
    float t = (float)s_anim.frame * ANIM_TIMER_PERIOD_MS / 1000.0f;
    uint32_t now_ms = s_anim.frame * ANIM_TIMER_PERIOD_MS;

    /* Get base parameters and copy to working set */
    const mochi_face_params_t *base = mochi_get_base_params();
    mochi_face_params_t *params = mochi_get_current_params();

    if (base == NULL || params == NULL) return;

    /* Start with base params */
    memcpy(params, base, sizeof(mochi_face_params_t));

    /* Apply state-specific animations first */
    apply_state_animation(params, t);

    /* Apply activity animation */
    switch (s_anim.current_activity) {
        case MOCHI_ACTIVITY_IDLE:
            apply_idle_animation(params, t);
            break;
        case MOCHI_ACTIVITY_SHAKE:
            apply_shake_animation(params, t);
            break;
        case MOCHI_ACTIVITY_BOUNCE:
            apply_bounce_animation(params, t);
            break;
        case MOCHI_ACTIVITY_SPIN:
            apply_spin_animation(params, t);
            break;
        case MOCHI_ACTIVITY_WIGGLE:
            apply_wiggle_animation(params, t);
            break;
        case MOCHI_ACTIVITY_NOD:
            apply_nod_animation(params, t);
            break;
        case MOCHI_ACTIVITY_BLINK:
            apply_idle_animation(params, t);
            apply_blink_animation(params, now_ms);
            break;
        case MOCHI_ACTIVITY_SNORE:
            apply_snore_animation(params, t);
            break;
        case MOCHI_ACTIVITY_VIBRATE:
            apply_vibrate_animation(params, t);
            break;
        default:
            apply_idle_animation(params, t);
            break;
    }

    /* Request face redraw */
    mochi_request_redraw();
}

/*===========================================================================
 * Forward Declarations
 *===========================================================================*/

void mochi_anim_stop(void);

/*===========================================================================
 * Public Functions
 *===========================================================================*/

void mochi_anim_init(void) {
    if (s_anim.initialized) return;

    ESP_LOGI(TAG, "Initializing animation controller");

    s_anim.frame = 0;
    s_anim.last_blink_ms = 0;
    s_anim.is_blinking = false;
    s_anim.running = false;
    s_anim.paused = false;
    s_anim.timer = NULL;

    s_anim.initialized = true;
}

void mochi_anim_deinit(void) {
    if (!s_anim.initialized) return;

    ESP_LOGI(TAG, "Deinitializing animation controller");

    mochi_anim_stop();
    s_anim.initialized = false;
}

void mochi_anim_start(mochi_state_t state, mochi_activity_t activity) {
    if (!s_anim.initialized) return;

    s_anim.current_state = state;
    s_anim.current_activity = activity;

    /* Create timer if not exists */
    if (s_anim.timer == NULL) {
        s_anim.timer = lv_timer_create(anim_timer_cb, ANIM_TIMER_PERIOD_MS, NULL);
        if (s_anim.timer == NULL) {
            ESP_LOGE(TAG, "Failed to create animation timer");
            return;
        }
    }

    s_anim.running = true;
    s_anim.paused = false;
    lv_timer_resume(s_anim.timer);

    ESP_LOGI(TAG, "Animation started: state=%d, activity=%d", state, activity);
}

void mochi_anim_stop(void) {
    if (s_anim.timer != NULL) {
        lv_timer_delete(s_anim.timer);
        s_anim.timer = NULL;
    }
    s_anim.running = false;
}

void mochi_anim_pause(void) {
    if (!s_anim.running) return;

    s_anim.paused = true;
    if (s_anim.timer != NULL) {
        lv_timer_pause(s_anim.timer);
    }
}

void mochi_anim_resume(void) {
    if (!s_anim.running) return;

    s_anim.paused = false;
    if (s_anim.timer != NULL) {
        lv_timer_resume(s_anim.timer);
    }
}

void mochi_anim_set_intensity(float intensity) {
    if (intensity < 0.2f) intensity = 0.2f;
    if (intensity > 1.0f) intensity = 1.0f;
    s_anim.intensity = intensity;
}
