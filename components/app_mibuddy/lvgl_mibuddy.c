/**
 * @file lvgl_mibuddy.c
 * @brief MiBuddy slideshow implementation
 *
 * Displays images from /sdcard/Images/ folder in a continuous
 * slideshow loop with 2-second intervals between images.
 * Supported formats: PNG only (max 240x284 recommended)
 */

#include "lvgl_mibuddy.h"
#include "bsp_board.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "mibuddy";

/* Declare the embedded wallpaper image for testing */
LV_IMAGE_DECLARE(esp_brookesia_image_small_wallpaper_dark_240_240);

/* Flag to show embedded test image first */
static bool s_show_embedded_first = true;

/* Maximum number of images to scan */
#define MAX_IMAGES 50

/* Slideshow interval in milliseconds */
#define SLIDESHOW_INTERVAL_MS 2000

/* Image directory on SD card (POSIX path for file scanning) */
#define IMAGES_DIR "/sdcard/Images"

/* LVGL filesystem path prefix (S: maps to /sdcard via LV_FS_POSIX_PATH) */
#define LVGL_IMAGES_DIR "S:Images"

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

    /* Cleanup any existing timer first (prevents multiple timers if reopened without cleanup) */
    if (s_slideshow_timer != NULL) {
        ESP_LOGW(TAG, "Deleting orphaned slideshow timer");
        lv_timer_delete(s_slideshow_timer);
        s_slideshow_timer = NULL;
    }

    /* Reset state */
    s_current_index = 0;
    s_image_count = 0;
    s_show_embedded_first = true;

    /* Scan SD card for image files (PNG, JPG, BMP, GIF) */
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
        lv_label_set_text(s_info_label, "No images found!\n\nPlace PNG files in:\n/sdcard/Images/\n\n(max 240x284)");
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
    s_show_embedded_first = true;  /* Reset to show embedded image first on next open */
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

    /* Only PNG supported */
    static const char *extensions[] = {".png"};
    static const int num_extensions = sizeof(extensions) / sizeof(extensions[0]);

    s_image_count = 0;

    /* Scan for each supported image format */
    for (int i = 0; i < num_extensions && s_image_count < MAX_IMAGES; i++) {
        uint16_t found = Folder_retrieval(
            IMAGES_DIR,
            extensions[i],
            &s_image_names[s_image_count],  /* Append to existing list */
            MAX_IMAGES - s_image_count       /* Remaining slots */
        );
        if (found > 0) {
            ESP_LOGI(TAG, "Found %d %s files", found, extensions[i]);
            s_image_count += found;
        }
    }

    if (s_image_count == 0) {
        ESP_LOGW(TAG, "No image files found in %s", IMAGES_DIR);
    } else {
        ESP_LOGI(TAG, "Found %d total images", s_image_count);
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
    if (s_img_obj == NULL) {
        ESP_LOGW(TAG, "display_current_image: img_obj is NULL");
        return;
    }

    /* Show embedded test image first to verify display works */
    if (s_show_embedded_first) {
        ESP_LOGI(TAG, "Displaying embedded wallpaper (240x240) as test");
        lv_image_set_src(s_img_obj, &esp_brookesia_image_small_wallpaper_dark_240_240);
        s_show_embedded_first = false;  /* Only show once */
        return;
    }

    /* No SD card images available */
    if (s_image_count == 0) {
        ESP_LOGW(TAG, "No SD card images to display");
        return;
    }

    /* Build full path to the image file
     * LVGL expects paths in format: "S:Images/filename.png"
     * where S: maps to /sdcard via LV_FS_POSIX_PATH config
     */
    static char img_path[MAX_PATH_SIZE + 8];
    snprintf(img_path, sizeof(img_path), "%s/%s",
             LVGL_IMAGES_DIR, s_image_names[s_current_index]);

    ESP_LOGI(TAG, "Displaying image [%d/%d]: %s",
             s_current_index + 1, s_image_count, img_path);

    /* Test: verify LVGL filesystem can open the file */
    lv_fs_file_t f;
    lv_fs_res_t res = lv_fs_open(&f, img_path, LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK) {
        ESP_LOGE(TAG, "LVGL fs_open failed: %s (error=%d)", img_path, res);
    } else {
        uint32_t size = 0;
        lv_fs_seek(&f, 0, LV_FS_SEEK_END);
        lv_fs_tell(&f, &size);
        ESP_LOGI(TAG, "LVGL fs_open OK: %s (size=%lu)", img_path, (unsigned long)size);
        lv_fs_close(&f);
    }

    /* Set the image source to the file path */
    lv_image_set_src(s_img_obj, img_path);

    /* Check if the image loaded correctly by checking dimensions */
    lv_image_header_t header;
    lv_result_t img_res = lv_image_decoder_get_info(img_path, &header);
    if (img_res != LV_RESULT_OK) {
        ESP_LOGE(TAG, "Failed to load image: %s (decoder error)", img_path);
    } else {
        ESP_LOGI(TAG, "Image loaded: %dx%d, cf=%d", header.w, header.h, header.cf);
    }
}

/**
 * @brief Timer callback to advance the slideshow
 *
 * @param timer LVGL timer handle
 */
static void slideshow_timer_cb(lv_timer_t *timer)
{
    /* Safety check: if image object was destroyed, stop the timer */
    if (s_img_obj == NULL) {
        ESP_LOGW(TAG, "Timer callback: img_obj is NULL, deleting orphaned timer");
        if (timer != NULL) {
            lv_timer_delete(timer);
        }
        s_slideshow_timer = NULL;
        return;
    }

    if (s_image_count == 0) {
        return;
    }

    /* Advance to next image, loop back to first */
    s_current_index = (s_current_index + 1) % s_image_count;

    /* Display the new image */
    display_current_image();
}
