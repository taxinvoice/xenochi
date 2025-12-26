/**
 * @file lvgl_app_car_gallery.cpp
 * @brief Car Animation Gallery - Main app implementation
 *
 * Provides a gallery view of 32 car-themed animations using the mochi system.
 */

#include "lvgl_app_car_gallery.hpp"
#include "car_gallery_data.h"
#include "app_car_gallery_assets.h"
#include "mochi_state.h"
#include "esp_log.h"
#include "private/esp_brookesia_utils.h"

static const char *TAG = "CarGallery";

/*===========================================================================
 * Constructor / Destructor
 *===========================================================================*/

PhoneCarGalleryConf::PhoneCarGalleryConf(bool use_status_bar, bool use_navigation_bar):
    ESP_Brookesia_PhoneApp("Car Gallery", &icon_car_gallery, true, use_status_bar, use_navigation_bar)
{
}

PhoneCarGalleryConf::PhoneCarGalleryConf():
    ESP_Brookesia_PhoneApp("Car Gallery", &icon_car_gallery, true)
{
}

PhoneCarGalleryConf::~PhoneCarGalleryConf()
{
}

/*===========================================================================
 * App Lifecycle
 *===========================================================================*/

bool PhoneCarGalleryConf::run(void)
{
    ESP_LOGI(TAG, "Car Gallery app starting");

    /* Force clean mochi state - ensures we create fresh on THIS screen
     * This handles the case where another app (MiBuddy) left mochi initialized
     * on a different screen, or previous Car Gallery session wasn't fully cleaned */
    mochi_deinit();

    /* Initialize mochi state system */
    if (mochi_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize mochi system");
        return false;
    }

    /* Create gallery UI */
    if (car_gallery_ui_init(lv_screen_active()) != 0) {
        ESP_LOGE(TAG, "Failed to create gallery UI");
        mochi_deinit();
        return false;
    }

    ESP_LOGI(TAG, "Car Gallery app started successfully");
    return true;
}

bool PhoneCarGalleryConf::back(void)
{
    ESP_LOGI(TAG, "Car Gallery app back");

    /* Cleanup */
    car_gallery_ui_deinit();
    mochi_deinit();

    /* Notify core to close app */
    ESP_BROOKESIA_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
    return true;
}

bool PhoneCarGalleryConf::close(void)
{
    ESP_LOGI(TAG, "Car Gallery app close");

    car_gallery_ui_deinit();
    mochi_deinit();

    return true;
}

bool PhoneCarGalleryConf::pause(void)
{
    ESP_LOGI(TAG, "Car Gallery app pause - full cleanup to release memory");

    /* Full cleanup when paused (switching to another app)
     * This releases mochi resources so other apps can use them */
    car_gallery_ui_deinit();
    mochi_deinit();

    return true;
}

bool PhoneCarGalleryConf::resume(void)
{
    ESP_LOGI(TAG, "Car Gallery app resume - reinitializing");

    /* Full reinitialization when resuming
     * Clean up any state from other apps first */
    mochi_deinit();

    if (mochi_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reinitialize mochi system");
        return false;
    }

    if (car_gallery_ui_init(lv_screen_active()) != 0) {
        ESP_LOGE(TAG, "Failed to reinitialize gallery UI");
        mochi_deinit();
        return false;
    }

    return true;
}
