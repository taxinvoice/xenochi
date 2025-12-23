/**
 * @file lvgl_app_mibuddy.cpp
 * @brief MiBuddy App implementation for ESP-Brookesia Phone UI
 *
 * This application displays a slideshow of PNG images from the SD card.
 * Future: Will be extended to show a virtual buddy that reacts to inputs.
 *
 * Lifecycle:
 * - run(): Called when app launches - scans SD card and starts slideshow
 * - back(): Called on back button - closes the app
 * - close(): Called on app exit - stops slideshow and cleans up
 */

#include "lvgl_app_mibuddy.hpp"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "private/esp_brookesia_utils.h"

#include "app_mibuddy_assets.h"
#include "lvgl_mibuddy.h"

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
 * Scans the SD card for PNG images and creates the slideshow UI.
 *
 * @return true on success
 */
bool PhoneMiBuddyConf::run(void)
{
    ESP_BROOKESIA_LOGD("Run");

    /* Create the slideshow UI on the active screen */
    lvgl_mibuddy_create(lv_screen_active());

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

    /* Stop slideshow before closing */
    lvgl_mibuddy_cleanup();

    /* Notify core to close the app */
    ESP_BROOKESIA_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

/**
 * @brief Called when the app is closed
 *
 * Stops the slideshow timer and cleans up resources.
 *
 * @return true on success
 */
bool PhoneMiBuddyConf::close(void)
{
    ESP_BROOKESIA_LOGD("Close");

    /* Notify core that app is closing */
    ESP_BROOKESIA_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    /* Cleanup slideshow resources */
    lvgl_mibuddy_cleanup();

    return true;
}
