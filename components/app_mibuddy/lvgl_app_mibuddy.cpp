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

#if 0  /* Temporarily disabled for PNG loading debug */
    /* Initialize and create mochi avatar */
    mochi_init();
    mochi_create(lv_screen_active());

    /* Start with happy idle state */
    mochi_set(MOCHI_STATE_HAPPY, MOCHI_ACTIVITY_IDLE);
#endif

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

#if 0  /* Temporarily disabled for PNG loading debug */
    /* Cleanup mochi resources before closing */
    mochi_deinit();
#endif

#if 0  /* Temporarily disabled */
    /* Cleanup slideshow resources */
    lvgl_mibuddy_cleanup();
#endif

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

#if 0  /* Temporarily disabled for PNG loading debug */
    /* Cleanup mochi resources FIRST before notifying core */
    mochi_deinit();
#endif

#if 0  /* Temporarily disabled */
    /* Cleanup slideshow resources */
    lvgl_mibuddy_cleanup();
#endif

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

#if 0  /* Temporarily disabled for PNG loading debug */
    /* Pause mochi when app is paused */
    mochi_pause();
#endif

#if 0  /* Temporarily disabled */
    /* Pause slideshow timer */
    lvgl_mibuddy_pause();
#endif

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

#if 0  /* Temporarily disabled for PNG loading debug */
    /* Resume mochi when app is resumed */
    mochi_resume();
#endif

#if 0  /* Temporarily disabled */
    /* Resume slideshow timer */
    lvgl_mibuddy_resume();
#endif

    return true;
}
