/**
 * @file lvgl_app_car_gallery.hpp
 * @brief Car Animation Gallery - Browse and preview car-themed animations
 *
 * This app provides a gallery view of all 32 car-related animation states
 * that can be used in a vehicle. Touch to navigate between animations.
 */
#pragma once

#include "systems/phone/esp_brookesia_phone_app.hpp"

/**
 * @brief Car Animation Gallery phone app class
 *
 * Displays a gallery of car-themed animations using the mochi state system.
 * Touch left/right to navigate, swipe for smooth transitions.
 */
class PhoneCarGalleryConf : public ESP_Brookesia_PhoneApp {
public:
    /**
     * @brief Construct Car Gallery app with status/navigation bar options
     *
     * @param use_status_bar Show phone status bar (0 = no)
     * @param use_navigation_bar Show navigation bar (0 = no)
     */
    PhoneCarGalleryConf(bool use_status_bar, bool use_navigation_bar);

    /**
     * @brief Construct Car Gallery app with default settings
     */
    PhoneCarGalleryConf();

    /**
     * @brief Destructor
     */
    ~PhoneCarGalleryConf();

protected:
    /**
     * @brief Called when the app is launched
     *
     * Creates the gallery UI and initializes the mochi system.
     *
     * @return true on success
     */
    bool run(void) override;

    /**
     * @brief Handle back button press
     *
     * Returns to launcher.
     *
     * @return true on success
     */
    bool back(void) override;

    /**
     * @brief Called when the app is closed
     *
     * Cleans up resources.
     *
     * @return true on success
     */
    bool close(void) override;

    /**
     * @brief Called when the app is paused
     *
     * @return true on success
     */
    bool pause(void) override;

    /**
     * @brief Called when the app is resumed
     *
     * @return true on success
     */
    bool resume(void) override;
};
