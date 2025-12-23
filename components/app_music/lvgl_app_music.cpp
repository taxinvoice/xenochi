/**
 * @file lvgl_app_music.cpp
 * @brief Music Player Application for ESP-Brookesia Phone UI
 *
 * This application provides music playback functionality:
 * - Browse music files from SD card
 * - Play/pause/stop audio files
 * - Volume control
 * - Track navigation
 *
 * The app uses the audio_driver module for playback and displays
 * a LVGL-based music player UI.
 *
 * Lifecycle:
 * - run(): Called when app launches - initializes audio and creates UI
 * - back(): Called on back button - closes the app
 * - close(): Called on app exit - cleans up audio resources
 */

#include "lvgl_app_music.hpp"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "private/esp_brookesia_utils.h"

#include "app_music_assets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsp_board.h"
#include "lvgl_music.h"
#include "audio_driver.h"

using namespace std;
using namespace esp_brookesia::gui;

/*===========================================================================
 * Constructors/Destructor
 *===========================================================================*/

/**
 * @brief Construct music app with status/navigation bar options
 *
 * @param use_status_bar Show phone status bar (0 = no)
 * @param use_navigation_bar Show navigation bar (0 = no)
 */
PhoneMusicConf::PhoneMusicConf(bool use_status_bar, bool use_navigation_bar):
    ESP_Brookesia_PhoneApp("Music", &icon_music, true, use_status_bar, use_navigation_bar)
{
}

/**
 * @brief Construct music app with default settings
 */
PhoneMusicConf::PhoneMusicConf():
    ESP_Brookesia_PhoneApp("Music", &icon_music, true)
{
}

/**
 * @brief Destructor
 */
PhoneMusicConf::~PhoneMusicConf()
{
    ESP_BROOKESIA_LOGD("Destroy(@0x%p)", this);
}

/*===========================================================================
 * App Lifecycle Methods
 *===========================================================================*/

/**
 * @brief Called when the app is launched
 *
 * Initializes the audio playback system and creates the music player UI.
 *
 * @return true on success
 */
bool PhoneMusicConf::run(void)
{
    ESP_BROOKESIA_LOGD("Run");

    /* Initialize audio playback subsystem (command queue, task, pipeline) */
    Audio_Play_Init();

    /* Note: LVGL_Search_Music() is called once at startup in main.cpp,
     * so no need to rescan here - just create the UI */

    /* Create the music player UI on the active screen */
    lvgl_music_create(lv_screen_active());

    return true;
}

/**
 * @brief Handle back button press
 *
 * Notifies the phone core to close this app and return to home screen.
 *
 * @return true on success
 */
bool PhoneMusicConf::back(void)
{
    ESP_BROOKESIA_LOGD("Back");

    /* Notify core to close the app */
    ESP_BROOKESIA_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

/**
 * @brief Called when the app is closed
 *
 * Cleans up audio resources by deinitializing the audio playback system.
 *
 * @return true on success
 */
bool PhoneMusicConf::close(void)
{
    ESP_BROOKESIA_LOGD("Close");

    /* Notify core that app is closing */
    ESP_BROOKESIA_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    /* Cleanup audio resources */
    Audio_Play_Deinit();

    return true;
}

// bool PhoneAppComplexConf::init()
// {
//     ESP_BROOKESIA_LOGD("Init");

//     /* Do some initialization here if needed */

//     return true;
// }

// bool PhoneAppComplexConf::deinit()
// {
//     ESP_BROOKESIA_LOGD("Deinit");

//     /* Do some deinitialization here if needed */

//     return true;
// }

// bool PhoneAppComplexConf::pause()
// {
//     ESP_BROOKESIA_LOGD("Pause");

//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppComplexConf::resume()
// {
//     ESP_BROOKESIA_LOGD("Resume");

//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppComplexConf::cleanResource()
// {
//     ESP_BROOKESIA_LOGD("Clean resource");

//     /* Do some cleanup here if needed */

//     return true;
// }
