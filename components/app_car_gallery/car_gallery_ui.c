/**
 * @file car_gallery_ui.c
 * @brief Car Animation Gallery - UI implementation with touch navigation
 *
 * Layout (240x284):
 * - Header (40px): Back button, animation name, counter
 * - Preview (200px): Mochi face animation
 * - Info panel (44px): State + activity, trigger description
 *
 * Touch zones:
 * - Tap left third: Previous animation
 * - Tap right third: Next animation
 * - Swipe left/right: Navigate with transition
 * - Long press: Open category picker
 */

#include "car_gallery_data.h"
#include "gallery_animations.h"
#include "mochi_state.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>

static const char *TAG = "CarGalleryUI";

/*===========================================================================
 * Configuration
 *===========================================================================*/

#define SCREEN_WIDTH     240
#define SCREEN_HEIGHT    284
#define HEADER_HEIGHT    40
#define INFO_HEIGHT      44
#define PREVIEW_HEIGHT   (SCREEN_HEIGHT - HEADER_HEIGHT - INFO_HEIGHT)

/* Touch zones */
#define ZONE_LEFT_MAX    80
#define ZONE_RIGHT_MIN   160

/*===========================================================================
 * Static Variables
 *===========================================================================*/

/* UI elements - created directly on screen, no blocking container */
static lv_obj_t *s_screen = NULL;  /* Reference to parent screen */
static lv_obj_t *s_header = NULL;
static lv_obj_t *s_back_btn = NULL;
static lv_obj_t *s_title_label = NULL;
static lv_obj_t *s_counter_label = NULL;
static lv_obj_t *s_preview_area = NULL;
static lv_obj_t *s_info_panel = NULL;
static lv_obj_t *s_state_label = NULL;
static lv_obj_t *s_trigger_label = NULL;
static lv_obj_t *s_category_overlay = NULL;

/* Gallery state */
static int s_current_idx = 0;
static car_category_t s_current_category = CAR_CAT_ALL;
static uint8_t s_filtered_indices[68];  /* 32 face + 36 custom animations */
static int s_filtered_count = 0;
static animation_type_t s_current_type = ANIM_TYPE_FACE;

/* Styles */
static lv_style_t s_style_header;
static lv_style_t s_style_info;
static lv_style_t s_style_btn;

/*===========================================================================
 * Forward Declarations
 *===========================================================================*/

static void update_filter(void);
static void apply_current_animation(void);
static void update_ui_labels(void);
static void preview_click_cb(lv_event_t *e);
static void preview_gesture_cb(lv_event_t *e);
static void long_press_cb(lv_event_t *e);
static void back_btn_cb(lv_event_t *e);
static void category_btn_cb(lv_event_t *e);

/*===========================================================================
 * Style Initialization
 *===========================================================================*/

static void init_styles(void)
{
    /* Header style - dark background */
    lv_style_init(&s_style_header);
    lv_style_set_bg_color(&s_style_header, lv_color_hex(0x1a1a2e));
    lv_style_set_bg_opa(&s_style_header, LV_OPA_COVER);
    lv_style_set_border_width(&s_style_header, 0);
    lv_style_set_radius(&s_style_header, 0);
    lv_style_set_pad_all(&s_style_header, 4);

    /* Info panel style - semi-transparent dark */
    lv_style_init(&s_style_info);
    lv_style_set_bg_color(&s_style_info, lv_color_black());
    lv_style_set_bg_opa(&s_style_info, LV_OPA_70);
    lv_style_set_border_width(&s_style_info, 0);
    lv_style_set_radius(&s_style_info, 0);
    lv_style_set_pad_all(&s_style_info, 4);

    /* Button style */
    lv_style_init(&s_style_btn);
    lv_style_set_bg_color(&s_style_btn, lv_color_hex(0x3949ab));
    lv_style_set_bg_opa(&s_style_btn, LV_OPA_COVER);
    lv_style_set_radius(&s_style_btn, 8);
    lv_style_set_text_color(&s_style_btn, lv_color_white());
}

/*===========================================================================
 * Header Creation
 *===========================================================================*/

static void create_header(lv_obj_t *parent)
{
    s_header = lv_obj_create(parent);
    lv_obj_remove_style_all(s_header);
    lv_obj_add_style(s_header, &s_style_header, 0);
    lv_obj_set_size(s_header, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_align(s_header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(s_header, LV_OBJ_FLAG_SCROLLABLE);

    /* Back button */
    s_back_btn = lv_btn_create(s_header);
    lv_obj_set_size(s_back_btn, 36, 32);
    lv_obj_align(s_back_btn, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_add_style(s_back_btn, &s_style_btn, 0);
    lv_obj_add_event_cb(s_back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(s_back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    /* Title label */
    s_title_label = lv_label_create(s_header);
    lv_obj_set_style_text_color(s_title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_title_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_title_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(s_title_label, "Animation");

    /* Counter label */
    s_counter_label = lv_label_create(s_header);
    lv_obj_set_style_text_color(s_counter_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(s_counter_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_counter_label, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_label_set_text(s_counter_label, "1/32");
}

/*===========================================================================
 * Preview Area Creation
 *===========================================================================*/

static void create_preview_area(lv_obj_t *parent)
{
    s_preview_area = lv_obj_create(parent);
    lv_obj_remove_style_all(s_preview_area);
    lv_obj_set_size(s_preview_area, SCREEN_WIDTH, PREVIEW_HEIGHT);
    lv_obj_align(s_preview_area, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT);
    lv_obj_clear_flag(s_preview_area, LV_OBJ_FLAG_SCROLLABLE);

    /* Transparent background - mochi renders here */
    lv_obj_set_style_bg_opa(s_preview_area, LV_OPA_TRANSP, 0);

    /* Touch event handlers */
    lv_obj_add_flag(s_preview_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_preview_area, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(s_preview_area, preview_click_cb, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(s_preview_area, preview_gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(s_preview_area, long_press_cb, LV_EVENT_LONG_PRESSED, NULL);
}

/*===========================================================================
 * Info Panel Creation
 *===========================================================================*/

static void create_info_panel(lv_obj_t *parent)
{
    s_info_panel = lv_obj_create(parent);
    lv_obj_remove_style_all(s_info_panel);
    lv_obj_add_style(s_info_panel, &s_style_info, 0);
    lv_obj_set_size(s_info_panel, SCREEN_WIDTH, INFO_HEIGHT);
    lv_obj_align(s_info_panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(s_info_panel, LV_OBJ_FLAG_SCROLLABLE);

    /* State + Activity label */
    s_state_label = lv_label_create(s_info_panel);
    lv_obj_set_style_text_color(s_state_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_state_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_state_label, LV_ALIGN_TOP_MID, 0, 2);
    lv_label_set_text(s_state_label, "HAPPY + IDLE");

    /* Trigger description */
    s_trigger_label = lv_label_create(s_info_panel);
    lv_obj_set_style_text_color(s_trigger_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(s_trigger_label, &lv_font_montserrat_18, 0);
    lv_obj_align(s_trigger_label, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_label_set_text(s_trigger_label, "Default state");
}

/*===========================================================================
 * Category Picker Overlay
 *===========================================================================*/

static void close_category_picker(void)
{
    if (s_category_overlay) {
        lv_obj_del(s_category_overlay);
        s_category_overlay = NULL;
    }
}

/* Forward declaration */
static void create_category_picker(void);

static void long_press_cb(lv_event_t *e)
{
    (void)e;
    create_category_picker();
}

static void category_btn_cb(lv_event_t *e)
{
    intptr_t cat = (intptr_t)lv_event_get_user_data(e);
    s_current_category = (car_category_t)cat;
    update_filter();
    close_category_picker();
}

static void overlay_click_cb(lv_event_t *e)
{
    /* Close on tap outside buttons */
    if (lv_event_get_target(e) == s_category_overlay) {
        close_category_picker();
    }
}

static void create_category_picker(void)
{
    if (s_category_overlay) return; /* Already open */

    /* Semi-transparent overlay */
    s_category_overlay = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(s_category_overlay);
    lv_obj_set_size(s_category_overlay, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(s_category_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_category_overlay, LV_OPA_70, 0);
    lv_obj_add_flag(s_category_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_category_overlay, overlay_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(s_category_overlay, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t *title = lv_label_create(s_category_overlay);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_label_set_text(title, "Select Category");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    /* Category buttons - 2 columns */
    int btn_w = 100;
    int btn_h = 32;
    int gap_x = 10;
    int gap_y = 8;
    int start_y = 80;
    int start_x = (SCREEN_WIDTH - (btn_w * 2 + gap_x)) / 2;

    for (int i = 0; i <= CAR_CAT_ALL; i++) {
        int col = i % 2;
        int row = i / 2;

        lv_obj_t *btn = lv_btn_create(s_category_overlay);
        lv_obj_set_size(btn, btn_w, btn_h);
        lv_obj_set_pos(btn, start_x + col * (btn_w + gap_x), start_y + row * (btn_h + gap_y));
        lv_obj_add_style(btn, &s_style_btn, 0);

        /* Highlight current category */
        if (i == s_current_category) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x5c6bc0), 0);
        }

        lv_obj_add_event_cb(btn, category_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, car_gallery_category_name((car_category_t)i));
        lv_obj_center(label);
    }
}

/*===========================================================================
 * Touch Event Handlers
 *===========================================================================*/

static void preview_click_cb(lv_event_t *e)
{
    lv_point_t point;
    lv_indev_get_point(lv_indev_active(), &point);

    ESP_LOGI(TAG, "Click at x=%d", point.x);

    if (point.x < ZONE_LEFT_MAX) {
        car_gallery_prev();
    } else if (point.x >= ZONE_RIGHT_MIN) {
        car_gallery_next();
    }
    /* Center tap - could toggle pause or show info */
}

static void preview_gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());

    if (dir == LV_DIR_LEFT) {
        ESP_LOGI(TAG, "Swipe left - next");
        car_gallery_next();
    } else if (dir == LV_DIR_RIGHT) {
        ESP_LOGI(TAG, "Swipe right - prev");
        car_gallery_prev();
    }
}

static void back_btn_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "Back button pressed");
    /* Trigger app back via event - the app handles cleanup */
    lv_obj_send_event(lv_screen_active(), LV_EVENT_CANCEL, NULL);
}

/*===========================================================================
 * Filter and Navigation
 *===========================================================================*/

static void update_filter(void)
{
    const car_animation_t *anims = car_gallery_get_animations();
    int total = car_gallery_get_count();

    s_filtered_count = 0;

    for (int i = 0; i < total; i++) {
        if (s_current_category == CAR_CAT_ALL || anims[i].category == s_current_category) {
            s_filtered_indices[s_filtered_count++] = i;
        }
    }

    /* Reset to first in filtered set */
    s_current_idx = 0;
    apply_current_animation();
}

static void apply_current_animation(void)
{
    if (s_filtered_count == 0) return;

    int actual_idx = s_filtered_indices[s_current_idx];
    const car_animation_t *anim = &car_gallery_get_animations()[actual_idx];

    /* Handle animation type switching */
    if (anim->type != s_current_type) {
        if (anim->type == ANIM_TYPE_FACE) {
            /* Switching to face animation - hide custom, show mochi */
            gallery_anim_set_visible(false);
            mochi_set_visible(true);
        } else {
            /* Switching to custom animation - hide mochi, show custom */
            mochi_set_visible(false);
            gallery_anim_set_visible(true);
        }
        s_current_type = anim->type;
    }

    /* Apply animation-specific settings */
    if (anim->type == ANIM_TYPE_FACE) {
        /* Set mochi state */
        mochi_set_theme(anim->theme);
        mochi_set(anim->state, anim->activity);
        ESP_LOGI(TAG, "Face %d: %s (%s + %s)",
                 s_current_idx + 1, anim->name,
                 mochi_state_name(anim->state),
                 mochi_activity_name(anim->activity));
    } else {
        /* Set custom animation */
        gallery_anim_set(anim->custom_id);
        const gallery_anim_info_t *info = gallery_anim_get_info(anim->custom_id);
        ESP_LOGI(TAG, "Custom %d: %s (%s)",
                 s_current_idx + 1, anim->name,
                 info ? info->name : "unknown");
    }

    /* Update labels */
    update_ui_labels();
}

static void update_ui_labels(void)
{
    if (s_filtered_count == 0) return;

    int actual_idx = s_filtered_indices[s_current_idx];
    const car_animation_t *anim = &car_gallery_get_animations()[actual_idx];

    /* Title */
    lv_label_set_text(s_title_label, anim->name);

    /* Counter */
    char buf[32];
    snprintf(buf, sizeof(buf), "%d/%d", s_current_idx + 1, s_filtered_count);
    lv_label_set_text(s_counter_label, buf);

    /* State info - different format for face vs custom */
    if (anim->type == ANIM_TYPE_FACE) {
        snprintf(buf, sizeof(buf), "%s + %s",
                 mochi_state_name(anim->state),
                 mochi_activity_name(anim->activity));
    } else {
        /* Show category for custom animations */
        snprintf(buf, sizeof(buf), "%s", car_gallery_category_name(anim->category));
    }
    lv_label_set_text(s_state_label, buf);

    /* Trigger */
    lv_label_set_text(s_trigger_label, anim->trigger_desc);
}

/*===========================================================================
 * Public API
 *===========================================================================*/

int car_gallery_ui_init(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "Initializing gallery UI");

    s_screen = parent;
    s_current_type = ANIM_TYPE_FACE;  /* Start with face animations */
    init_styles();

    /* Create mochi FIRST on screen - it renders behind UI elements
     * mochi_init() is already called by the app's run() method */
    esp_err_t err = mochi_create(parent);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create mochi: %d", err);
        return -1;
    }

    /* Initialize gallery animations module (for custom non-face animations) */
    if (gallery_anim_init(parent) != 0) {
        ESP_LOGE(TAG, "Failed to init gallery animations");
        /* Continue anyway - face animations will still work */
    }
    /* Start with gallery_anim hidden (face animations visible first) */
    gallery_anim_set_visible(false);

    /* Create UI elements directly on screen (no blocking container)
     * Elements are created AFTER mochi so they render on top */
    create_preview_area(parent);  /* Touch handling overlay */
    create_header(parent);
    create_info_panel(parent);

    /* Bring UI elements to front to ensure they're on top of mochi */
    lv_obj_move_foreground(s_header);
    lv_obj_move_foreground(s_info_panel);
    lv_obj_move_foreground(s_preview_area);

    /* Initialize filter and show first animation */
    update_filter();

    ESP_LOGI(TAG, "Gallery UI initialized with %d animations", s_filtered_count);
    return 0;
}

void car_gallery_ui_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing gallery UI");

    close_category_picker();

    /* Cleanup gallery animations module */
    gallery_anim_deinit();

    /* Delete UI elements (mochi is cleaned up by app's close/back) */
    if (s_header) {
        lv_obj_del(s_header);
        s_header = NULL;
    }
    if (s_info_panel) {
        lv_obj_del(s_info_panel);
        s_info_panel = NULL;
    }
    if (s_preview_area) {
        lv_obj_del(s_preview_area);
        s_preview_area = NULL;
    }

    /* Delete styles */
    lv_style_reset(&s_style_header);
    lv_style_reset(&s_style_info);
    lv_style_reset(&s_style_btn);

    /* Reset pointers */
    s_screen = NULL;
    s_back_btn = NULL;
    s_title_label = NULL;
    s_counter_label = NULL;
    s_state_label = NULL;
    s_trigger_label = NULL;

    /* Reset gallery state */
    s_current_type = ANIM_TYPE_FACE;
}

void car_gallery_next(void)
{
    if (s_filtered_count == 0) return;
    s_current_idx = (s_current_idx + 1) % s_filtered_count;
    apply_current_animation();
}

void car_gallery_prev(void)
{
    if (s_filtered_count == 0) return;
    s_current_idx = (s_current_idx - 1 + s_filtered_count) % s_filtered_count;
    apply_current_animation();
}

void car_gallery_set_category(car_category_t cat)
{
    if (cat >= CAR_CAT_MAX) cat = CAR_CAT_ALL;
    s_current_category = cat;
    update_filter();
}

int car_gallery_get_current_index(void)
{
    return s_current_idx;
}

const car_animation_t* car_gallery_get_current(void)
{
    if (s_filtered_count == 0) return NULL;
    int actual_idx = s_filtered_indices[s_current_idx];
    return &car_gallery_get_animations()[actual_idx];
}
