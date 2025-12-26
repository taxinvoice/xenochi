/**
 * @file lvgl_app_mibuddy.cpp
 * @brief MiBuddy App implementation for ESP-Brookesia Phone UI
 *
 * This application displays a cute mochi avatar with:
 * - 8 emotional states (Happy, Excited, Worried, Cool, Dizzy, Panic, Sleepy, Shocked)
 * - 9 activity animations (Idle, Shake, Bounce, Spin, Wiggle, Nod, Blink, Snore, Vibrate)
 * - 5 color themes (Sakura, Mint, Lavender, Peach, Cloud)
 * - Particle effects
 *
 * Lifecycle:
 * - run(): Called when app launches - creates mochi avatar
 * - back(): Called on back button - closes the app
 * - close(): Called on app exit - cleans up mochi resources
 */

#include "lvgl_app_mibuddy.hpp"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "esp_log.h"
#include "private/esp_brookesia_utils.h"
#include <cstdio>

#include "app_mibuddy_assets.h"
#include "mochi_state.h"
#include "mochi_input.h"
#include "mochi_assets.h"
#include "audio_driver.h"

/* External sounds */
extern "C" {
    /* Short beep - 8kHz, ~100ms */
    extern const int16_t beep_8k_mono[];
    extern const size_t beep_8k_mono_len;
    extern const uint32_t beep_8k_mono_sample_rate;

    /* Two-tone chime - 16kHz, ~400ms */
    extern const int16_t chime_16k_mono[];
    extern const size_t chime_16k_mono_len;
    extern const uint32_t chime_16k_mono_sample_rate;

    /* "Weee" sound for extreme roll - 16kHz */
    extern const int16_t weee_hahaha_16k[];
    extern const size_t weee_hahaha_16k_len;
    extern const uint32_t weee_hahaha_16k_sample_rate;

    /* Braking/caught sound - 16kHz */
    extern const int16_t sound_braking[];
    extern const size_t sound_braking_len;
    extern const uint32_t sound_braking_sample_rate;

    /* Face down/sleepy sound - 16kHz */
    extern const int16_t sound_face_down[];
    extern const size_t sound_face_down_len;
    extern const uint32_t sound_face_down_sample_rate;

    /* Low battery warning sound - 16kHz */
    extern const int16_t sound_low_battery[];
    extern const size_t sound_low_battery_len;
    extern const uint32_t sound_low_battery_sample_rate;
}


using namespace std;

/*===========================================================================
 * Input System Configuration
 *===========================================================================*/

/**
 * @brief Input mapper timer interval in milliseconds
 *
 * Default from Kconfig (CONFIG_MIBUDDY_INPUT_INTERVAL_MS).
 * Can be changed at runtime with mibuddy_set_input_interval().
 */
#ifdef CONFIG_MIBUDDY_INPUT_INTERVAL_MS
static uint32_t s_input_timer_interval_ms = CONFIG_MIBUDDY_INPUT_INTERVAL_MS;
#else
static uint32_t s_input_timer_interval_ms = 200;  /* Default 2000ms if Kconfig not set */
#endif

static lv_timer_t *s_input_timer = NULL;

/**
 * @brief Set the input mapper timer interval
 *
 * @param interval_ms Interval in milliseconds (min 50ms, max 5000ms)
 */
void mibuddy_set_input_interval(uint32_t interval_ms)
{
    /* Clamp to reasonable range */
    if (interval_ms < 50) interval_ms = 50;
    if (interval_ms > 5000) interval_ms = 5000;

    s_input_timer_interval_ms = interval_ms;
    ESP_LOGI("MiBuddy", "Input interval set to %lums (%.1f Hz)",
             (unsigned long)interval_ms, 1000.0f / interval_ms);

    /* Update running timer if it exists */
    if (s_input_timer != NULL) {
        lv_timer_set_period(s_input_timer, interval_ms);
        ESP_LOGI("MiBuddy", "Timer period updated");
    }
}

/**
 * @brief Get the current input mapper timer interval
 *
 * @return Interval in milliseconds
 */
uint32_t mibuddy_get_input_interval(void)
{
    return s_input_timer_interval_ms;
}

/**
 * @brief Default input mapper - Maps sensor inputs to mochi state/activity
 *
 * STATE MACHINE LOGIC (evaluated in priority order, first match wins):
 * ============================================================================
 *
 *  Priority | Condition              | State    | Activity    | Sound
 *  ---------|------------------------|----------|-------------|---------------
 *  1        | is_braking             | SHOCKED  | IDLE        | braking (edge)
 *  2        | is_face_down           | SLEEPY   | SNORE       | face_down (edge)
 *  3a       | roll < 55° (extreme L) | EXCITED  | SLIDE_LEFT  | weee (edge)
 *  3b       | roll < 65° (normal L)  | HAPPY    | SLIDE_LEFT  | -
 *  4a       | roll > 125° (extreme R)| EXCITED  | SLIDE_RIGHT | weee (edge)
 *  4b       | roll > 115° (normal R) | HAPPY    | SLIDE_RIGHT | -
 *  5        | is_low_battery         | WORRIED  | IDLE        | low_battery (edge)
 *  6        | (default)              | HAPPY    | IDLE        | -
 *
 * ROLL ANGLE ZONES (baseline = 90°):
 * ============================================================================
 *
 *     0°        55°    65°    90°   115°   125°       180°
 *     |----------|------|------|------|------|---------|
 *     | EXTREME  |NORMAL| neutral zone |NORMAL|EXTREME |
 *     |  LEFT    | LEFT |   (no roll)  |RIGHT | RIGHT  |
 *     | +sound   |      |              |      | +sound |
 *     | EXCITED  |HAPPY |    HAPPY     |HAPPY |EXCITED |
 *
 * EDGE-TRIGGERED SOUNDS:
 * ============================================================================
 *   - Sound plays ONCE when entering extreme zone (not continuously)
 *   - Resets when returning to normal roll zone (allows re-trigger)
 *   - Separate tracking for left/right to allow independent triggers
 */
static void default_input_mapper(
    const mochi_input_state_t *input,
    mochi_state_t *out_state,
    mochi_activity_t *out_activity)
{
    /* Edge-trigger state for sounds */
    static bool s_braking_sound_played = false;
    static bool s_face_down_sound_played = false;
    static bool s_low_battery_sound_played = false;

    /* Log motion state for testing */
    ESP_LOGI("TEST", "braking:%d delta:%.1f roll:%.1f low_batt:%d",
             input->is_braking, input->accel_delta_per_sec,
             input->roll, input->is_low_battery);

    /* 1. Braking (catching device) */
    if (input->is_braking) {
        if (!s_braking_sound_played) {
            ESP_LOGI("SOUND", ">>> Braking detected! Playing sound...");
            mochi_sound_asset_t snd = MOCHI_SOUND_EMBEDDED(sound_braking, sound_braking_len, sound_braking_sample_rate);
            mochi_play_asset_sound(&snd, false);
            s_braking_sound_played = true;
        }
        *out_state = MOCHI_STATE_SHOCKED;
        *out_activity = MOCHI_ACTIVITY_IDLE;
        return;
    }
    s_braking_sound_played = false;  /* Reset when not braking */

    /* 2. Face down */
    if (input->is_face_down) {
        if (!s_face_down_sound_played) {
            ESP_LOGI("SOUND", ">>> Face down! Playing sound...");
            mochi_sound_asset_t snd = MOCHI_SOUND_EMBEDDED(sound_face_down, sound_face_down_len, sound_face_down_sample_rate);
            mochi_play_asset_sound(&snd, false);
            s_face_down_sound_played = true;
        }
        *out_state = MOCHI_STATE_SLEEPY;
        *out_activity = MOCHI_ACTIVITY_SNORE;
        return;
    }
    s_face_down_sound_played = false;  /* Reset when not face down */

    /* 3-4. Roll tilt with two-tier detection (skip if face down/up) */
    static bool s_extreme_left_active = false;
    static bool s_extreme_right_active = false;

    if (!input->is_face_down && !input->is_face_up) {
        const float ROLL_BASELINE = 90.0f;
        const float ROLL_THRESHOLD = 25.0f;         /* Normal roll threshold */
        const float ROLL_EXTREME_THRESHOLD = 35.0f; /* Extreme roll (plays sound) */

        /* Left roll detection */
        if (input->roll < ROLL_BASELINE - ROLL_THRESHOLD) {
            /* Check for extreme left roll */
            if (input->roll < ROLL_BASELINE - ROLL_EXTREME_THRESHOLD) {
                /* Play sound on extreme left entry (edge trigger) */
                if (!s_extreme_left_active) {
                    ESP_LOGI("SOUND", ">>> Extreme left roll! Playing weee...");
                    mochi_sound_asset_t weee = MOCHI_SOUND_EMBEDDED(weee_hahaha_16k, weee_hahaha_16k_len, weee_hahaha_16k_sample_rate);
                    mochi_play_asset_sound(&weee, false);
                    s_extreme_left_active = true;
                }
                *out_state = MOCHI_STATE_EXCITED;
            } else {
                *out_state = MOCHI_STATE_HAPPY;
                s_extreme_left_active = false;  /* Reset when back to normal roll */
            }
            *out_activity = MOCHI_ACTIVITY_SLIDE_LEFT;
            return;
        }

        /* Right roll detection */
        if (input->roll > ROLL_BASELINE + ROLL_THRESHOLD) {
            /* Check for extreme right roll */
            if (input->roll > ROLL_BASELINE + ROLL_EXTREME_THRESHOLD) {
                /* Play sound on extreme right entry (edge trigger) */
                if (!s_extreme_right_active) {
                    ESP_LOGI("SOUND", ">>> Extreme right roll! Playing weee...");
                    mochi_sound_asset_t weee = MOCHI_SOUND_EMBEDDED(weee_hahaha_16k, weee_hahaha_16k_len, weee_hahaha_16k_sample_rate);
                    mochi_play_asset_sound(&weee, false);
                    s_extreme_right_active = true;
                }
                *out_state = MOCHI_STATE_EXCITED;
            } else {
                *out_state = MOCHI_STATE_HAPPY;
                s_extreme_right_active = false;  /* Reset when back to normal roll */
            }
            *out_activity = MOCHI_ACTIVITY_SLIDE_RIGHT;
            return;
        }
    }
    /* Reset both when not tilted */
    s_extreme_left_active = false;
    s_extreme_right_active = false;

    /* 5. Low battery */
    if (input->is_low_battery) {
        if (!s_low_battery_sound_played) {
            ESP_LOGI("SOUND", ">>> Low battery! Playing warning sound...");
            mochi_sound_asset_t snd = MOCHI_SOUND_EMBEDDED(sound_low_battery, sound_low_battery_len, sound_low_battery_sample_rate);
            mochi_play_asset_sound(&snd, false);
            s_low_battery_sound_played = true;
        }
        *out_state = MOCHI_STATE_WORRIED;
        *out_activity = MOCHI_ACTIVITY_IDLE;
        return;
    }
    s_low_battery_sound_played = false;  /* Reset when battery OK */

    /* 6. Default */
    *out_state = MOCHI_STATE_HAPPY;
    *out_activity = MOCHI_ACTIVITY_IDLE;
}

/*===========================================================================
 * State Hold Logic - Prevents jittery state changes
 *===========================================================================*/

static mochi_state_t s_held_state = MOCHI_STATE_HAPPY;
static mochi_activity_t s_held_activity = MOCHI_ACTIVITY_IDLE;
static uint32_t s_state_change_time = 0;

/* State hold time from Kconfig (default 0 = disabled) */
#ifdef CONFIG_MIBUDDY_STATE_HOLD_MS
static const uint32_t STATE_HOLD_MS = CONFIG_MIBUDDY_STATE_HOLD_MS;
#else
static const uint32_t STATE_HOLD_MS = 0;
#endif

/**
 * @brief Get state priority (lower = higher priority, can interrupt)
 */
static int get_state_priority(mochi_state_t state) {
    switch (state) {
        case MOCHI_STATE_PANIC:   return 0;  /* Highest - always interrupts */
        case MOCHI_STATE_DIZZY:   return 1;
        case MOCHI_STATE_SHOCKED: return 2;
        case MOCHI_STATE_WORRIED: return 3;
        case MOCHI_STATE_SLEEPY:  return 4;
        case MOCHI_STATE_EXCITED: return 5;
        case MOCHI_STATE_COOL:    return 6;
        case MOCHI_STATE_HAPPY:   return 7;  /* Lowest */
        default: return 99;
    }
}

/**
 * @brief Wrapper mapper that applies state hold logic
 *
 * Calls the real mapper, then applies hold timer logic:
 * - If hold time expired OR new state is higher priority: accept new state
 * - Otherwise: keep the held state (prevents jitter)
 */
static void hold_wrapper_mapper(
    const mochi_input_state_t *input,
    mochi_state_t *out_state,
    mochi_activity_t *out_activity)
{
    /* Get proposed state from real mapper */
    mochi_state_t proposed_state;
    mochi_activity_t proposed_activity;
    default_input_mapper(input, &proposed_state, &proposed_activity);

    /* Apply hold logic */
    uint32_t now = lv_tick_get();
    bool hold_expired = (STATE_HOLD_MS == 0) || ((now - s_state_change_time) >= STATE_HOLD_MS);
    bool higher_priority = get_state_priority(proposed_state) < get_state_priority(s_held_state);

    if (hold_expired || higher_priority) {
        /* Accept the new state */
        if (proposed_state != s_held_state || proposed_activity != s_held_activity) {
            s_held_state = proposed_state;
            s_held_activity = proposed_activity;
            s_state_change_time = now;
            ESP_LOGI("StateHold", "State changed: %s + %s (priority=%d, hold=%s)",
                     mochi_state_name(s_held_state), mochi_activity_name(s_held_activity),
                     get_state_priority(s_held_state), higher_priority ? "interrupted" : "expired");
        }
    }

    /* Return the held state (prevents jitter when hold active) */
    *out_state = s_held_state;
    *out_activity = s_held_activity;
}

/*===========================================================================
 * State Label Display
 *===========================================================================*/

static lv_obj_t *s_state_label = NULL;

static void update_state_label(void) {
    if (s_state_label == NULL) return;

    /* Get current state and activity from mochi system */
    mochi_state_t state = mochi_get_state();
    mochi_activity_t activity = mochi_get_activity();

    /* Format: "STATE + ACTIVITY" */
    static char label_buf[64];
    snprintf(label_buf, sizeof(label_buf), "%s + %s",
             mochi_state_name(state), mochi_activity_name(activity));
    lv_label_set_text(s_state_label, label_buf);
}

/*===========================================================================
 * Debug Overlay - Shows calculated input states
 *===========================================================================*/

static lv_obj_t *s_debug_overlay = NULL;
static lv_obj_t *s_debug_orient_label = NULL;
static lv_obj_t *s_debug_motion_label = NULL;
static lv_obj_t *s_debug_angles_label = NULL;
static lv_obj_t *s_debug_calc_label = NULL;

static void create_debug_overlay(lv_obj_t *parent) {
    /* Create semi-transparent light container behind the face */
    s_debug_overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(s_debug_overlay);
    lv_obj_set_size(s_debug_overlay, 240, 284);
    lv_obj_set_pos(s_debug_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_debug_overlay, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_debug_overlay, LV_OPA_40, 0);
    lv_obj_clear_flag(s_debug_overlay, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* Common label style - dark text on light background */
    static lv_style_t style_label;
    static bool style_init = false;
    if (!style_init) {
        lv_style_init(&style_label);
        lv_style_set_text_font(&style_label, &lv_font_montserrat_14);
        lv_style_set_text_color(&style_label, lv_color_make(64, 64, 64));  /* Dark gray */
        lv_style_set_text_opa(&style_label, LV_OPA_COVER);
        style_init = true;
    }

    /* Orientation label - top left */
    s_debug_orient_label = lv_label_create(s_debug_overlay);
    lv_obj_add_style(s_debug_orient_label, &style_label, 0);
    lv_obj_set_pos(s_debug_orient_label, 8, 45);
    lv_label_set_text(s_debug_orient_label, "ORIENT");

    /* Calculated bools label - below orientation */
    s_debug_calc_label = lv_label_create(s_debug_overlay);
    lv_obj_add_style(s_debug_calc_label, &style_label, 0);
    lv_obj_set_pos(s_debug_calc_label, 8, 65);
    lv_label_set_text(s_debug_calc_label, "");

    /* Motion label - top right */
    s_debug_motion_label = lv_label_create(s_debug_overlay);
    lv_obj_add_style(s_debug_motion_label, &style_label, 0);
    lv_obj_align(s_debug_motion_label, LV_ALIGN_TOP_RIGHT, -8, 45);
    lv_label_set_text(s_debug_motion_label, "MOTION");

    /* Angles label - bottom left (above nav bar) - bigger font */
    s_debug_angles_label = lv_label_create(s_debug_overlay);
    lv_obj_add_style(s_debug_angles_label, &style_label, 0);
    lv_obj_set_style_text_font(s_debug_angles_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_debug_angles_label, LV_ALIGN_BOTTOM_LEFT, 8, -45);
    lv_label_set_text(s_debug_angles_label, "P:0 R:0");

    /* Note: overlay is moved to foreground after mochi_create() in constructor */
}

static void update_debug_overlay(void) {
    if (s_debug_overlay == NULL) return;

    const mochi_input_state_t *input = mochi_input_get();
    if (input == NULL) return;

    static char buf[128];

    /* Orientation - show which direction */
    const char *orient = "-";
    if (input->is_face_up) orient = "UP";
    else if (input->is_face_down) orient = "DOWN";
    else if (input->is_portrait) orient = "PORT";
    else if (input->is_portrait_inv) orient = "PORT INV";
    else if (input->is_landscape_left) orient = "LANDL";
    else if (input->is_landscape_right) orient = "LANDR";
    lv_label_set_text(s_debug_orient_label, orient);

    /* Calculated boolean flags - show abbreviation if true, dash if false */
    snprintf(buf, sizeof(buf),
        "%s %s %s\n"
        "%s %s %s\n"
        "%s\n"
        "%.1fs",
        input->is_moving ? "MOV" : "---",
        input->is_shaking ? "SHK" : "---",
        input->is_night ? "NGT" : "---",
        input->is_idle ? "IDL" : "---",
        input->is_rotating ? "ROT" : "---",
        input->is_spinning ? "SPN" : "---",
        input->is_weekend ? "WKD" : "---",
        input->current_state_duration_ms / 1000.0f);
    lv_label_set_text(s_debug_calc_label, buf);

    /* Motion flags */
    snprintf(buf, sizeof(buf), "%s%s%s%s",
             input->is_moving ? "M" : "-",
             input->is_shaking ? "S" : "-",
             input->is_rotating ? "R" : "-",
             input->is_spinning ? "X" : "-");
    lv_label_set_text(s_debug_motion_label, buf);
    lv_obj_align(s_debug_motion_label, LV_ALIGN_TOP_RIGHT, -8, 45);

    /* Pitch and Roll angles with direction indicators */
    const char *pitch_dir = " ";
    if (input->pitch > 25.0f) pitch_dir = "^";       // SLIDE_UP threshold
    else if (input->pitch < -25.0f) pitch_dir = "v";  // SLIDE_DOWN threshold

    const char *roll_dir = "  ";
    if (input->roll < 65.0f) roll_dir = "<<";       // SLIDE_LEFT threshold
    else if (input->roll > 115.0f) roll_dir = ">>";  // SLIDE_RIGHT threshold

    snprintf(buf, sizeof(buf), "P:%+.0f%s R:%+.0f%s", input->pitch, pitch_dir, input->roll, roll_dir);
    lv_label_set_text(s_debug_angles_label, buf);
    lv_obj_align(s_debug_angles_label, LV_ALIGN_BOTTOM_LEFT, 8, -45);

}

/**
 * @brief Timer callback to update input state
 *
 * Called periodically to collect inputs and run the mapper.
 * Updates at 10Hz (100ms) for responsive state changes.
 */
static void input_timer_cb(lv_timer_t *t)
{
    mochi_input_update();
    update_state_label();    /* Update label after state change */
    update_debug_overlay();  /* Update debug overlay with input states */
}

/*===========================================================================
 * Constructors/Destructor
 *===========================================================================*/

/**
 * @brief Construct MiBuddy app with status/navigation bar options
 *
 * @param use_status_bar Show phone status bar (0 = no)
 * @param use_navigation_bar Show navigation bar (0 = no)
 */
PhoneMiBuddyConf::PhoneMiBuddyConf(bool use_status_bar, bool use_navigation_bar):
    ESP_Brookesia_PhoneApp("MiBuddy", &icon_mibuddy, true, use_status_bar, use_navigation_bar)
{
}

/**
 * @brief Construct MiBuddy app with default settings
 */
PhoneMiBuddyConf::PhoneMiBuddyConf():
    ESP_Brookesia_PhoneApp("MiBuddy", &icon_mibuddy, true)
{
}

/**
 * @brief Destructor
 */
PhoneMiBuddyConf::~PhoneMiBuddyConf()
{
    ESP_BROOKESIA_LOGD("Destroy(@0x%p)", this);
}

/*===========================================================================
 * App Lifecycle Methods
 *===========================================================================*/

/**
 * @brief Called when the app is launched
 *
 * Initializes and creates the mochi avatar.
 *
 * @return true on success
 */
bool PhoneMiBuddyConf::run(void)
{
    ESP_BROOKESIA_LOGD("Run");

    /* Initialize audio system (required for embedded PCM playback) */
    Audio_Play_Init();

    /* Create debug overlay */
    create_debug_overlay(lv_screen_active());

    /* Initialize and create mochi avatar
     * Note: Asset setup callback is registered in main.cpp and called during mochi_init()
     */
    mochi_init();
    mochi_create(lv_screen_active());

    /* Bring debug overlay to front AFTER face is created */
    lv_obj_move_foreground(s_debug_overlay);

    /* Create state label overlay at top of screen */
    s_state_label = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_color(s_state_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_state_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_bg_color(s_state_label, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_state_label, LV_OPA_70, 0);
    lv_obj_set_style_pad_all(s_state_label, 8, 0);
    lv_obj_set_style_radius(s_state_label, 6, 0);
    lv_obj_align(s_state_label, LV_ALIGN_TOP_MID, 0, 10);
    /* Bring label to front so it's always visible */
    lv_obj_move_foreground(s_state_label);
    update_state_label();

    /* Start with first state (Happy + Idle) */
    mochi_set(MOCHI_STATE_HAPPY, MOCHI_ACTIVITY_IDLE);

    /* Initialize input system with hold-wrapped mapper */
    mochi_input_init();
    mochi_input_set_mapper_fn(hold_wrapper_mapper);

    /* Set API URL for remote decisions (from Kconfig) */
#ifdef CONFIG_MIBUDDY_API_URL
    mochi_input_set_api_url(CONFIG_MIBUDDY_API_URL);
#else
    mochi_input_set_api_url("http://10.0.13.101:8080/mochi/state");
#endif

    /* Start timer for input updates */
    s_input_timer = lv_timer_create(input_timer_cb, s_input_timer_interval_ms, NULL);

    ESP_LOGI("MiBuddy", "Input mapper started: interval=%lums (%.1f Hz), API fallback when idle",
             (unsigned long)s_input_timer_interval_ms, 1000.0f / s_input_timer_interval_ms);

    return true;
}

/**
 * @brief Handle back button press
 *
 * Notifies the phone core to close this app and return to home screen.
 *
 * @return true on success
 */
bool PhoneMiBuddyConf::back(void)
{
    ESP_BROOKESIA_LOGD("Back");

    /* Stop input timer */
    if (s_input_timer) {
        lv_timer_delete(s_input_timer);
        s_input_timer = NULL;
    }

    /* Cleanup input system */
    mochi_input_deinit();

    /* Reset UI pointers (LVGL will delete them with screen) */
    s_state_label = NULL;
    s_debug_overlay = NULL;
    s_debug_orient_label = NULL;
    s_debug_motion_label = NULL;
    s_debug_angles_label = NULL;
    s_debug_calc_label = NULL;

    /* Reset state hold variables */
    s_held_state = MOCHI_STATE_HAPPY;
    s_held_activity = MOCHI_ACTIVITY_IDLE;
    s_state_change_time = 0;

    /* Cleanup mochi resources before closing */
    mochi_deinit();

    /* Notify core to close the app */
    ESP_BROOKESIA_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

/**
 * @brief Called when the app is closed
 *
 * Cleans up mochi resources.
 *
 * @return true on success
 */
bool PhoneMiBuddyConf::close(void)
{
    ESP_BROOKESIA_LOGD("Close");

    /* Stop input timer */
    if (s_input_timer) {
        lv_timer_delete(s_input_timer);
        s_input_timer = NULL;
    }

    /* Cleanup input system */
    mochi_input_deinit();

    /* Reset UI pointers (LVGL will delete them with screen) */
    s_state_label = NULL;
    s_debug_overlay = NULL;
    s_debug_orient_label = NULL;
    s_debug_motion_label = NULL;
    s_debug_angles_label = NULL;
    s_debug_calc_label = NULL;

    /* Reset state hold variables */
    s_held_state = MOCHI_STATE_HAPPY;
    s_held_activity = MOCHI_ACTIVITY_IDLE;
    s_state_change_time = 0;

    /* Cleanup mochi resources FIRST before notifying core */
    mochi_deinit();

    /* Notify core that app is closing */
    ESP_BROOKESIA_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

/**
 * @brief Called when the app is paused (e.g., switching to another app)
 *
 * Pauses mochi animations while app is in background.
 *
 * @return true on success
 */
bool PhoneMiBuddyConf::pause(void)
{
    ESP_BROOKESIA_LOGD("Pause");

    /* Stop input timer to disable state machine while paused */
    if (s_input_timer) {
        lv_timer_delete(s_input_timer);
        s_input_timer = NULL;
    }

    /* Pause mochi when app is paused */
    mochi_pause();

    return true;
}

/**
 * @brief Called when the app is resumed from pause
 *
 * Resumes mochi animations.
 *
 * @return true on success
 */
bool PhoneMiBuddyConf::resume(void)
{
    ESP_BROOKESIA_LOGD("Resume");

    /* Resume mochi when app is resumed */
    mochi_resume();

    /* Restart input timer for state machine */
    if (s_input_timer == NULL) {
        s_input_timer = lv_timer_create(input_timer_cb, s_input_timer_interval_ms, NULL);
    }

    return true;
}
