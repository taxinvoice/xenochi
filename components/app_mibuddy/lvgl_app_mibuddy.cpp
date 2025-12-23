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
#include "audio_driver.h"

extern "C" {
#include "lvgl_mibuddy.h"
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
 * @brief Default mapper function - maps inputs to mochi state
 *
 * This is a simple example mapper. Users can replace this with their own logic.
 * The mapper examines input state and decides which mochi state/activity to use.
 */
static void default_input_mapper(
    const mochi_input_state_t *input,
    mochi_state_t *out_state,
    mochi_activity_t *out_activity)
{
    static const char *MAP_TAG = "input_mapper";
    static int call_count = 0;
    call_count++;

    /* Log every 10th call to reduce spam, or every call if you want full verbosity */
    bool verbose = (call_count % 10 == 1);  /* Change to (true) for every call */

    if (verbose) {
        ESP_LOGI(MAP_TAG, "========================================");
        ESP_LOGI(MAP_TAG, "MAPPER CALL #%d", call_count);
        ESP_LOGI(MAP_TAG, "========================================");

        /* ── Static Variables ── */
        ESP_LOGI(MAP_TAG, "--- STATIC INPUTS ---");
        ESP_LOGI(MAP_TAG, "Battery: %.1f%%, charging: %s, temp: %.1f°C",
                 input->battery_pct,
                 input->is_charging ? "YES" : "NO",
                 input->temperature);
        ESP_LOGI(MAP_TAG, "Time: %02d:%02d, day_of_week: %d",
                 input->hour, input->minute, input->day_of_week);
        ESP_LOGI(MAP_TAG, "Accel: X=%.2f Y=%.2f Z=%.2f g",
                 input->accel_x, input->accel_y, input->accel_z);
        ESP_LOGI(MAP_TAG, "Gyro: X=%.1f Y=%.1f Z=%.1f deg/s",
                 input->gyro_x, input->gyro_y, input->gyro_z);
        ESP_LOGI(MAP_TAG, "WiFi: %s, Touch: %s",
                 input->wifi_connected ? "CONNECTED" : "disconnected",
                 input->touch_active ? "ACTIVE" : "inactive");

        /* ── Calculated Variables ── */
        ESP_LOGI(MAP_TAG, "--- CALCULATED ---");

        /* Motion */
        ESP_LOGI(MAP_TAG, "accel_mag: %.2fg, gyro_mag: %.1f°/s",
                 input->accel_magnitude, input->gyro_magnitude);
        ESP_LOGI(MAP_TAG, "is_moving: %s | is_shaking: %s | is_rotating: %s | is_spinning: %s",
                 input->is_moving ? "YES" : "no",
                 input->is_shaking ? "YES" : "no",
                 input->is_rotating ? "YES" : "no",
                 input->is_spinning ? "YES" : "no");

        /* Orientation */
        const char *orient = "unknown";
        if (input->is_face_up) orient = "FACE_UP";
        else if (input->is_face_down) orient = "FACE_DOWN";
        else if (input->is_portrait) orient = "PORTRAIT";
        else if (input->is_portrait_inv) orient = "PORTRAIT_INV";
        else if (input->is_landscape_left) orient = "LANDSCAPE_LEFT";
        else if (input->is_landscape_right) orient = "LANDSCAPE_RIGHT";
        ESP_LOGI(MAP_TAG, "Orientation: %s, pitch: %.1f°, roll: %.1f°",
                 orient, input->pitch, input->roll);

        /* Battery/Time */
        ESP_LOGI(MAP_TAG, "low_batt: %s | critical: %s | night: %s | weekend: %s",
                 input->is_low_battery ? "YES" : "no",
                 input->is_critical_battery ? "YES" : "no",
                 input->is_night ? "YES" : "no",
                 input->is_weekend ? "YES" : "no");

        ESP_LOGI(MAP_TAG, "--- DECISION LOGIC ---");
    }

    /* Priority order: most urgent conditions first */

    /* Shaking device -> PANIC */
    if (input->is_shaking) {
        if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_shaking=TRUE -> PANIC+VIBRATE");
        *out_state = MOCHI_STATE_PANIC;
        *out_activity = MOCHI_ACTIVITY_VIBRATE;
        if (verbose) ESP_LOGI(MAP_TAG, ">>> RESULT: %s + %s",
                              mochi_state_name(*out_state), mochi_activity_name(*out_activity));
        return;
    }
    if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_shaking=false, continue...");

    /* Spinning fast -> DIZZY */
    if (input->is_spinning) {
        if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_spinning=TRUE -> DIZZY+SPIN");
        *out_state = MOCHI_STATE_DIZZY;
        *out_activity = MOCHI_ACTIVITY_SPIN;
        if (verbose) ESP_LOGI(MAP_TAG, ">>> RESULT: %s + %s",
                              mochi_state_name(*out_state), mochi_activity_name(*out_activity));
        return;
    }
    if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_spinning=false, continue...");

    /* Critical battery -> WORRIED */
    if (input->is_critical_battery) {
        if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_critical_battery=TRUE -> WORRIED+IDLE");
        *out_state = MOCHI_STATE_WORRIED;
        *out_activity = MOCHI_ACTIVITY_IDLE;
        if (verbose) ESP_LOGI(MAP_TAG, ">>> RESULT: %s + %s",
                              mochi_state_name(*out_state), mochi_activity_name(*out_activity));
        return;
    }
    if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_critical_battery=false, continue...");

    /* Face down -> SLEEPY (device put down to rest) */
    if (input->is_face_down) {
        if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_face_down=TRUE -> SLEEPY+SNORE");
        *out_state = MOCHI_STATE_SLEEPY;
        *out_activity = MOCHI_ACTIVITY_SNORE;
        if (verbose) ESP_LOGI(MAP_TAG, ">>> RESULT: %s + %s",
                              mochi_state_name(*out_state), mochi_activity_name(*out_activity));
        return;
    }
    if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_face_down=false, continue...");

    /* Upside down -> SHOCKED */
    if (input->is_portrait_inv) {
        if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_portrait_inv=TRUE -> SHOCKED+WIGGLE");
        *out_state = MOCHI_STATE_SHOCKED;
        *out_activity = MOCHI_ACTIVITY_WIGGLE;
        if (verbose) ESP_LOGI(MAP_TAG, ">>> RESULT: %s + %s",
                              mochi_state_name(*out_state), mochi_activity_name(*out_activity));
        return;
    }
    if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_portrait_inv=false, continue...");

    /* Night time -> SLEEPY */
    if (input->is_night) {
        if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_night=TRUE -> SLEEPY+SNORE");
        *out_state = MOCHI_STATE_SLEEPY;
        *out_activity = MOCHI_ACTIVITY_SNORE;
        if (verbose) ESP_LOGI(MAP_TAG, ">>> RESULT: %s + %s",
                              mochi_state_name(*out_state), mochi_activity_name(*out_activity));
        return;
    }
    if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_night=false, continue...");

    /* Rotating gently -> COOL with NOD */
    if (input->is_rotating) {
        if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_rotating=TRUE -> COOL+NOD");
        *out_state = MOCHI_STATE_COOL;
        *out_activity = MOCHI_ACTIVITY_NOD;
        if (verbose) ESP_LOGI(MAP_TAG, ">>> RESULT: %s + %s",
                              mochi_state_name(*out_state), mochi_activity_name(*out_activity));
        return;
    }
    if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_rotating=false, continue...");

    /* Moving around -> EXCITED */
    if (input->is_moving) {
        if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_moving=TRUE -> EXCITED+BOUNCE");
        *out_state = MOCHI_STATE_EXCITED;
        *out_activity = MOCHI_ACTIVITY_BOUNCE;
        if (verbose) ESP_LOGI(MAP_TAG, ">>> RESULT: %s + %s",
                              mochi_state_name(*out_state), mochi_activity_name(*out_activity));
        return;
    }
    if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_moving=false, continue...");

    /* Landscape orientation -> COOL (relaxed viewing) */
    if (input->is_landscape_left || input->is_landscape_right) {
        if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_landscape=TRUE -> COOL+IDLE");
        *out_state = MOCHI_STATE_COOL;
        *out_activity = MOCHI_ACTIVITY_IDLE;
        if (verbose) ESP_LOGI(MAP_TAG, ">>> RESULT: %s + %s",
                              mochi_state_name(*out_state), mochi_activity_name(*out_activity));
        return;
    }
    if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_landscape=false, continue...");

    /* Low battery (but not critical) -> WORRIED */
    if (input->is_low_battery) {
        if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_low_battery=TRUE -> WORRIED+IDLE");
        *out_state = MOCHI_STATE_WORRIED;
        *out_activity = MOCHI_ACTIVITY_IDLE;
        if (verbose) ESP_LOGI(MAP_TAG, ">>> RESULT: %s + %s",
                              mochi_state_name(*out_state), mochi_activity_name(*out_activity));
        return;
    }
    if (verbose) ESP_LOGI(MAP_TAG, "CHECK: is_low_battery=false, continue...");

    /* Default: Check for API result or request async query */
    if (input->wifi_connected) {
        /* First check if we have a pending API result (non-blocking) */
        if (mochi_input_get_api_result(out_state, out_activity)) {
            if (verbose) ESP_LOGI(MAP_TAG, ">>> ASYNC API RESULT: %s + %s",
                                  mochi_state_name(*out_state), mochi_activity_name(*out_activity));
            return;  /* API decided */
        }

        /* No result yet - request a query (non-blocking, runs in background task) */
        if (verbose) ESP_LOGI(MAP_TAG, "CHECK: wifi_connected=TRUE, requesting async API...");
        mochi_input_request_api_query(input);
    } else {
        if (verbose) ESP_LOGI(MAP_TAG, "CHECK: wifi_connected=false, skipping API");
    }

    /* Fallback: HAPPY (while waiting for API or if offline) */
    if (verbose) ESP_LOGI(MAP_TAG, "FALLBACK: No conditions met -> HAPPY+IDLE");
    *out_state = MOCHI_STATE_HAPPY;
    *out_activity = MOCHI_ACTIVITY_IDLE;
    if (verbose) ESP_LOGI(MAP_TAG, ">>> RESULT: %s + %s",
                          mochi_state_name(*out_state), mochi_activity_name(*out_activity));
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

/**
 * @brief Timer callback to update input state
 *
 * Called periodically to collect inputs and run the mapper.
 * Updates at 10Hz (100ms) for responsive state changes.
 */
static void input_timer_cb(lv_timer_t *t)
{
    mochi_input_update();
    update_state_label();  /* Update label after state change */
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

    /* Initialize audio system before mochi (which may play sounds) */
    Audio_Play_Init();

    /* Initialize and create mochi avatar
     * Note: Asset setup callback is registered in main.cpp and called during mochi_init()
     */
    mochi_init();
    mochi_create(lv_screen_active());

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

    /* Initialize input system with default mapper */
    mochi_input_init();
    mochi_input_set_mapper_fn(default_input_mapper);

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

#if 0  /* Temporarily disabled */
    /* Create the slideshow UI (shows embedded images, then SD card PNGs) */
    lvgl_mibuddy_create(lv_screen_active());
#endif

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

    /* Reset label pointer (LVGL will delete it with screen) */
    s_state_label = NULL;

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

    /* Reset label pointer (LVGL will delete it with screen) */
    s_state_label = NULL;

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

    return true;
}
