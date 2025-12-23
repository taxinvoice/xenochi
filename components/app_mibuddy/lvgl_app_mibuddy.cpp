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
#include "private/esp_brookesia_utils.h"

#include "app_mibuddy_assets.h"
#include "mochi_state.h"

extern "C" {
#include "lvgl_mibuddy.h"
}

using namespace std;

/*===========================================================================
 * State Cycling for Touch Demo
 *===========================================================================*/

static int s_current_state = 0;
static int s_current_activity = 0;
static lv_obj_t *s_state_label = NULL;

static void update_state_label(void) {
    if (s_state_label == NULL) return;

    lv_label_set_text(s_state_label, mochi_state_name((mochi_state_t)s_current_state));
}

static void cycle_to_next_state(void) {
    s_current_state = (s_current_state + 1) % MOCHI_STATE_MAX;
    ESP_BROOKESIA_LOGI("Mochi: %s", mochi_state_name((mochi_state_t)s_current_state));
    update_state_label();
    mochi_set((mochi_state_t)s_current_state, MOCHI_ACTIVITY_IDLE);
}

static void cycle_to_prev_state(void) {
    s_current_state = (s_current_state + MOCHI_STATE_MAX - 1) % MOCHI_STATE_MAX;
    ESP_BROOKESIA_LOGI("Mochi: %s", mochi_state_name((mochi_state_t)s_current_state));
    update_state_label();
    mochi_set((mochi_state_t)s_current_state, MOCHI_ACTIVITY_IDLE);
}

static void on_screen_click(lv_event_t *e) {
    /* Single click - go to next state */
    cycle_to_next_state();
}

static void on_screen_gesture(lv_event_t *e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());

    if (dir == LV_DIR_BOTTOM) {
        /* Swipe down - go to next state */
        cycle_to_next_state();
    } else if (dir == LV_DIR_TOP) {
        /* Swipe up - go to previous state */
        cycle_to_prev_state();
    }
    /* Ignore left/right swipes */
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

    /* Reset state and activity index */
    s_current_state = 0;
    s_current_activity = 0;

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

    ESP_BROOKESIA_LOGI("Mochi: %s - TAP or SWIPE to change state",
                       mochi_state_name((mochi_state_t)s_current_state));

    /* Add touch handlers to cycle through states */
    lv_obj_t *screen = lv_screen_active();
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen, on_screen_click, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(screen, on_screen_gesture, LV_EVENT_GESTURE, NULL);

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
