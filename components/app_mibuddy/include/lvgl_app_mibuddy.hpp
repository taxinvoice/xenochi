/**
 * @file lvgl_app_mibuddy.hpp
 * @brief MiBuddy App - Virtual buddy that displays images from SD card
 *
 * This app shows a slideshow of PNG images from /sdcard/Images/ folder,
 * cycling through them every 2 seconds in a continuous loop.
 */
#pragma once

#include "systems/phone/esp_brookesia_phone_app.hpp"

/**
 * @brief MiBuddy phone app class
 *
 * Displays images from SD card in a slideshow format.
 * Future: Will be extended to show a virtual buddy that reacts to inputs.
 */
class PhoneMiBuddyConf : public ESP_Brookesia_PhoneApp {
public:
    /**
     * @brief Construct MiBuddy app with status/navigation bar options
     *
     * @param use_status_bar Show phone status bar (0 = no)
     * @param use_navigation_bar Show navigation bar (0 = no)
     */
    PhoneMiBuddyConf(bool use_status_bar, bool use_navigation_bar);

    /**
     * @brief Construct MiBuddy app with default settings
     */
    PhoneMiBuddyConf();

    /**
     * @brief Destructor
     */
    ~PhoneMiBuddyConf();

protected:
    /**
     * @brief Called when the app is launched
     *
     * Scans SD card for images and starts the slideshow.
     *
     * @return true on success
     */
    bool run(void) override;

    /**
     * @brief Handle back button press
     *
     * Notifies the phone core to close this app and return to home screen.
     *
     * @return true on success
     */
    bool back(void) override;

    /**
     * @brief Called when the app is closed
     *
     * Stops the slideshow timer and cleans up resources.
     *
     * @return true on success
     */
    bool close(void) override;

    /**
     * @brief Called when the app is paused (switching to another app)
     *
     * Stops the slideshow timer while app is in background.
     *
     * @return true on success
     */
    bool pause(void) override;

    /**
     * @brief Called when the app is resumed from pause
     *
     * Restarts the slideshow timer.
     *
     * @return true on success
     */
    bool resume(void) override;
};
