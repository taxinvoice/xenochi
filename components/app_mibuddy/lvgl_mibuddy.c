/**
 * @file lvgl_mibuddy.c
 * @brief MiBuddy slideshow implementation
 *
 * Displays PNG images from /sdcard/Images/ folder in a continuous
 * slideshow loop with 2-second intervals between images.
 */

#include "lvgl_mibuddy.h"
#include "bsp_board.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "mibuddy";

/* Maximum number of images to scan */
#define MAX_IMAGES 50

/* Slideshow interval in milliseconds */
#define SLIDESHOW_INTERVAL_MS 2000

/* Image directory on SD card */
#define IMAGES_DIR "/sdcard/Images"

/* LVGL file path prefix for SD card
 * LVGL uses drive letter format - check if configured for 'S' or use default */
#define LVGL_SDCARD_PREFIX "S:"

/*===========================================================================
 * Static Variables
 *===========================================================================*/

/* Array to store found image filenames */
static char s_image_names[MAX_IMAGES][MAX_FILE_NAME_SIZE];

/* Number of images found */
static uint16_t s_image_count = 0;

/* Current image index in slideshow */
static uint16_t s_current_index = 0;

/* LVGL image object for displaying images */
static lv_obj_t *s_img_obj = NULL;

/* LVGL timer for slideshow advancement */
static lv_timer_t *s_slideshow_timer = NULL;

/* Label for showing status/info */
static lv_obj_t *s_info_label = NULL;

/*===========================================================================
 * Static Function Prototypes
 *===========================================================================*/

static void scan_images(void);
static void display_current_image(void);
static void slideshow_timer_cb(lv_timer_t *timer);

/*===========================================================================
 * Public Functions
 *===========================================================================*/

/**
 * @brief Create the MiBuddy slideshow UI
 *
 * @param parent The parent LVGL object to create the UI on
 */
void lvgl_mibuddy_create(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "Creating MiBuddy slideshow UI");

    /* Reset state */
    s_current_index = 0;
    s_image_count = 0;

    /* Scan SD card for PNG images */
    scan_images();

    /* Create container for the slideshow */
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    if (s_image_count == 0) {
        /* No images found - show info message */
        s_info_label = lv_label_create(container);
        lv_label_set_text(s_info_label, "No images found!\n\nPlace PNG files in:\n/sdcard/Images/");
        lv_obj_set_style_text_color(s_info_label, lv_color_white(), 0);
        lv_obj_set_style_text_align(s_info_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(s_info_label);

        ESP_LOGW(TAG, "No images found in %s", IMAGES_DIR);
        return;
    }

    ESP_LOGI(TAG, "Found %d images, starting slideshow", s_image_count);

    /* Create image object for slideshow */
    s_img_obj = lv_image_create(container);
    lv_obj_set_size(s_img_obj, LV_PCT(100), LV_PCT(100));
    lv_obj_center(s_img_obj);

    /* Set image scaling mode to fit the display while maintaining aspect ratio */
    lv_image_set_inner_align(s_img_obj, LV_IMAGE_ALIGN_CENTER);
    lv_image_set_scale(s_img_obj, 256);  /* Default scale 1.0 = 256 */

    /* Display the first image */
    display_current_image();

    /* Create timer for slideshow advancement */
    s_slideshow_timer = lv_timer_create(slideshow_timer_cb, SLIDESHOW_INTERVAL_MS, NULL);
    if (s_slideshow_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create slideshow timer");
    }
}

/**
 * @brief Cleanup MiBuddy resources
 */
void lvgl_mibuddy_cleanup(void)
{
    ESP_LOGI(TAG, "Cleaning up MiBuddy resources");

    /* Delete the slideshow timer */
    if (s_slideshow_timer != NULL) {
        lv_timer_delete(s_slideshow_timer);
        s_slideshow_timer = NULL;
    }

    /* Reset state */
    s_img_obj = NULL;
    s_info_label = NULL;
    s_current_index = 0;
}

/*===========================================================================
 * Static Functions
 *===========================================================================*/

/**
 * @brief Scan SD card for PNG images
 */
static void scan_images(void)
{
    ESP_LOGI(TAG, "Scanning for images in %s", IMAGES_DIR);

    /* Use the BSP folder retrieval function to find PNG files */
    s_image_count = Folder_retrieval(
        IMAGES_DIR,
        ".png",
        s_image_names,
        MAX_IMAGES
    );

    if (s_image_count == 0) {
        ESP_LOGW(TAG, "No PNG files found in %s", IMAGES_DIR);
    } else {
        ESP_LOGI(TAG, "Found %d PNG files", s_image_count);
        for (int i = 0; i < s_image_count && i < 5; i++) {
            ESP_LOGI(TAG, "  [%d] %s", i, s_image_names[i]);
        }
        if (s_image_count > 5) {
            ESP_LOGI(TAG, "  ... and %d more", s_image_count - 5);
        }
    }
}

/**
 * @brief Display the current image in the slideshow
 */
static void display_current_image(void)
{
    if (s_img_obj == NULL || s_image_count == 0) {
        return;
    }

    /* Build full path to the image file
     * LVGL expects paths in format: "S:/sdcard/Images/filename.png"
     * The "S:" prefix tells LVGL to use the STDIO file driver
     */
    static char img_path[MAX_PATH_SIZE + 8];
    snprintf(img_path, sizeof(img_path), "%s%s/%s",
             LVGL_SDCARD_PREFIX, IMAGES_DIR, s_image_names[s_current_index]);

    ESP_LOGI(TAG, "Displaying image [%d/%d]: %s",
             s_current_index + 1, s_image_count, img_path);

    /* Set the image source to the file path */
    lv_image_set_src(s_img_obj, img_path);
}

/**
 * @brief Timer callback to advance the slideshow
 *
 * @param timer LVGL timer handle
 */
static void slideshow_timer_cb(lv_timer_t *timer)
{
    (void)timer;  /* Unused */

    if (s_image_count == 0) {
        return;
    }

    /* Advance to next image, loop back to first */
    s_current_index = (s_current_index + 1) % s_image_count;

    /* Display the new image */
    display_current_image();
}
