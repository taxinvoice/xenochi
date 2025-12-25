/**
 * @file mochi_face.c
 * @brief MochiState Face Rendering - Draw the cute mochi avatar face
 *
 * Uses LVGL canvas for custom drawing of:
 * - Mochi-shaped face with shadow/highlight
 * - Expressive eyes with pupils and sparkles
 * - Multiple mouth types
 * - Blush circles
 */

#include "mochi_state.h"
#include "mochi_theme.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include <math.h>

static const char *TAG = "mochi_face";

/*===========================================================================
 * Display Dimensions
 *===========================================================================*/

#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  284

/*===========================================================================
 * Canvas Size Configuration
 * Change CANVAS_SIZE to adjust face size. All dimensions scale automatically.
 * Memory usage: CANVAS_SIZE * CANVAS_SIZE * 2 bytes (RGB565)
 *   80  -> ~12.8KB
 *   100 -> ~20KB
 *   120 -> ~28.8KB
 *   150 -> ~45KB
 *===========================================================================*/
#define CANVAS_SIZE     120

/* Canvas dimensions */
#define CANVAS_WIDTH    CANVAS_SIZE
#define CANVAS_HEIGHT   CANVAS_SIZE

/* Scale factor: original design was 200x200 */
#define SCALE(x)        ((x) * CANVAS_SIZE / 200)

/* Face dimensions - relative to canvas center */
#define FACE_CENTER_X   (CANVAS_WIDTH / 2)
#define FACE_CENTER_Y   (CANVAS_HEIGHT / 2)
#define FACE_RADIUS_X   SCALE(85)
#define FACE_RADIUS_Y   SCALE(75)

/* Eye positions (relative to face center) */
#define LEFT_EYE_X      SCALE(-35)
#define RIGHT_EYE_X     SCALE(35)
#define EYE_Y           SCALE(-10)
#define EYE_WIDTH       SCALE(22)
#define EYE_HEIGHT      SCALE(28)

/* Mouth position */
#define MOUTH_Y         SCALE(40)

/* Blush position */
#define BLUSH_X         SCALE(55)
#define BLUSH_Y         SCALE(20)
#define BLUSH_RX        SCALE(18)
#define BLUSH_RY        SCALE(10)

/*===========================================================================
 * Static Variables
 *===========================================================================*/

static lv_obj_t *s_canvas = NULL;
static lv_draw_buf_t *s_draw_buf = NULL;
static bool s_visible = true;

/* Cached params for redraw */
static mochi_face_params_t s_cached_params;
static const mochi_theme_t *s_cached_theme = NULL;

/*===========================================================================
 * Drawing Helper Functions
 *===========================================================================*/

/**
 * @brief Draw a filled ellipse
 */
static void draw_ellipse(lv_layer_t *layer, int cx, int cy, int rx, int ry, lv_color_t color, lv_opa_t opa) {
    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = color;
    arc_dsc.opa = opa;
    arc_dsc.width = rx;  /* Use width as radius for filled effect */
    arc_dsc.center.x = cx;
    arc_dsc.center.y = cy;
    arc_dsc.start_angle = 0;
    arc_dsc.end_angle = 360;

    /* For filled ellipse, draw multiple arcs with decreasing radius */
    /* This is a workaround since LVGL doesn't have direct filled ellipse */

    /* Use filled rectangle with rounded corners for approximate ellipse */
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = color;
    rect_dsc.bg_opa = opa;
    rect_dsc.radius = LV_RADIUS_CIRCLE;
    rect_dsc.border_width = 0;

    lv_area_t area = {
        .x1 = cx - rx,
        .y1 = cy - ry,
        .x2 = cx + rx,
        .y2 = cy + ry,
    };
    lv_draw_rect(layer, &rect_dsc, &area);
}

/**
 * @brief Draw a filled circle
 */
static void draw_circle(lv_layer_t *layer, int cx, int cy, int r, lv_color_t color, lv_opa_t opa) {
    draw_ellipse(layer, cx, cy, r, r, color, opa);
}

/**
 * @brief Draw a line
 */
static void draw_line(lv_layer_t *layer, int x1, int y1, int x2, int y2, lv_color_t color, int width) {
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = color;
    line_dsc.width = width;
    line_dsc.round_start = true;
    line_dsc.round_end = true;
    line_dsc.opa = LV_OPA_COVER;
    line_dsc.p1.x = x1;
    line_dsc.p1.y = y1;
    line_dsc.p2.x = x2;
    line_dsc.p2.y = y2;

    lv_draw_line(layer, &line_dsc);
}

/*===========================================================================
 * Face Drawing Functions
 *===========================================================================*/

/**
 * @brief Clear the canvas with white background
 */
static void draw_background(lv_layer_t *layer, const mochi_theme_t *theme) {
    (void)theme;  /* Not using theme background */

    /* Fill with white background (RGB565 doesn't support transparency) */
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = lv_color_white();
    rect_dsc.bg_opa = LV_OPA_COVER;
    rect_dsc.radius = 0;

    lv_area_t area = {0, 0, CANVAS_WIDTH - 1, CANVAS_HEIGHT - 1};
    lv_draw_rect(layer, &rect_dsc, &area);
}

/**
 * @brief Draw the main face shape
 */
static void draw_face(lv_layer_t *layer, const mochi_face_params_t *p, const mochi_theme_t *theme) {
    int cx = FACE_CENTER_X + (int)p->face_offset_x;
    int cy = FACE_CENTER_Y + (int)p->face_offset_y;

    /* Apply squish effect */
    float scale_x = 1.0f + p->face_squish;
    float scale_y = 1.0f - p->face_squish;
    int rx = (int)(FACE_RADIUS_X * scale_x);
    int ry = (int)(FACE_RADIUS_Y * scale_y);

    /* Face shadow */
    draw_ellipse(layer, cx + 3, cy + 5, rx + 3, ry + 3, theme->face_shadow, LV_OPA_30);

    /* Main face */
    draw_ellipse(layer, cx, cy, rx, ry, theme->face, LV_OPA_COVER);

    /* Face highlight (top-left) */
    draw_ellipse(layer, cx - 15, cy - 35, 40, 20, theme->face_highlight, LV_OPA_50);
}

/**
 * @brief Draw the blush circles
 */
static void draw_blush(lv_layer_t *layer, const mochi_face_params_t *p, const mochi_theme_t *theme) {
    if (!p->show_blush) return;

    int cx = FACE_CENTER_X + (int)p->face_offset_x;
    int cy = FACE_CENTER_Y + (int)p->face_offset_y + BLUSH_Y;

    /* Left blush - layered for soft effect */
    for (int i = 0; i < 3; i++) {
        draw_ellipse(layer, cx - BLUSH_X + i, cy,
                     BLUSH_RX - i * 3, BLUSH_RY - i * 2,
                     theme->blush, LV_OPA_60 - i * 10);
    }

    /* Right blush */
    for (int i = 0; i < 3; i++) {
        draw_ellipse(layer, cx + BLUSH_X - i, cy,
                     BLUSH_RX - i * 3, BLUSH_RY - i * 2,
                     theme->blush, LV_OPA_60 - i * 10);
    }
}

/**
 * @brief Draw a single eye
 */
static void draw_eye(lv_layer_t *layer, int cx, int cy, bool is_right,
                     const mochi_face_params_t *p, const mochi_theme_t *theme) {
    /* Apply offsets */
    int ex = cx + (int)p->eye_offset_x;
    int ey = cy + (int)p->eye_offset_y;

    /* Calculate dimensions with scale and squish */
    int eye_w = (int)(EYE_WIDTH * p->eye_scale);
    int eye_h = (int)(EYE_HEIGHT * p->eye_scale * (1.0f - p->eye_squish));
    int pupil_w = (int)(10 * p->pupil_size * p->eye_scale);
    int pupil_h = (int)(12 * p->pupil_size * p->eye_scale);

    /* Minimum height for nearly closed eyes */
    if (eye_h < 4) eye_h = 4;

    /* Eye background */
    draw_ellipse(layer, ex, ey, eye_w, eye_h, theme->eye, LV_OPA_COVER);

    /* Pupil (if eyes not too squished) */
    if (eye_h > 8) {
        int pupil_offset_x = (int)(p->eye_offset_x * 0.15f);
        int pupil_offset_y = (int)(p->eye_offset_y * 0.1f);
        draw_ellipse(layer, ex + pupil_offset_x, ey + 2 + pupil_offset_y,
                     pupil_w, pupil_h, theme->pupil, LV_OPA_COVER);
    }

    /* Main highlight */
    if (eye_h > 10) {
        int hl_x = is_right ? ex - 7 : ex - 7;
        int hl_y = ey - 10;
        int hl_r = (int)(7 * p->eye_scale);
        draw_circle(layer, hl_x + (int)(p->eye_offset_x * 0.5f),
                    hl_y + (int)(p->eye_offset_y * 0.3f),
                    hl_r, lv_color_white(), LV_OPA_COVER);
    }

    /* Small sparkle */
    if (p->show_sparkles && eye_h > 12) {
        int sp_x = is_right ? ex + 5 : ex + 5;
        int sp_y = ey + 5;
        int sp_r = (int)(3 * p->eye_scale);
        draw_circle(layer, sp_x + (int)(p->eye_offset_x * 0.3f),
                    sp_y + (int)(p->eye_offset_y * 0.2f),
                    sp_r, theme->accent, LV_OPA_80);
    }
}

/**
 * @brief Draw both eyes
 */
static void draw_eyes(lv_layer_t *layer, const mochi_face_params_t *p, const mochi_theme_t *theme) {
    int cx = FACE_CENTER_X + (int)p->face_offset_x;
    int cy = FACE_CENTER_Y + (int)p->face_offset_y + EYE_Y;

    /* Left eye */
    draw_eye(layer, cx + LEFT_EYE_X, cy, false, p, theme);

    /* Right eye */
    draw_eye(layer, cx + RIGHT_EYE_X, cy, true, p, theme);
}

/**
 * @brief Draw the mouth
 */
static void draw_mouth(lv_layer_t *layer, const mochi_face_params_t *p, const mochi_theme_t *theme) {
    int cx = FACE_CENTER_X + (int)p->face_offset_x;
    int cy = FACE_CENTER_Y + (int)p->face_offset_y + MOUTH_Y;
    float open = p->mouth_open;

    int lw = SCALE(15) > 0 ? SCALE(15) : 1;  /* Line width, min 1 */

    switch (p->mouth_type) {
        case MOCHI_MOUTH_SMILE:
            /* Curved smile - draw as two lines meeting at center */
            draw_line(layer, cx - SCALE(20), cy, cx, cy + (int)(SCALE(12) * open), theme->mouth, lw);
            draw_line(layer, cx, cy + (int)(SCALE(12) * open), cx + SCALE(20), cy, theme->mouth, lw);
            break;

        case MOCHI_MOUTH_OPEN_SMILE:
            /* Open mouth */
            draw_ellipse(layer, cx, cy + SCALE(5), (int)(SCALE(18) * open), (int)(SCALE(15) * open),
                         theme->mouth, LV_OPA_COVER);
            break;

        case MOCHI_MOUTH_SMALL_O:
            /* Small O shape */
            draw_ellipse(layer, cx, cy, (int)(SCALE(10) * open), (int)(SCALE(12) * open),
                         theme->mouth, LV_OPA_COVER);
            break;

        case MOCHI_MOUTH_SMIRK:
            /* Angled smirk */
            draw_line(layer, cx - SCALE(15), cy + SCALE(5), cx + SCALE(20), cy - SCALE(8), theme->mouth, lw);
            break;

        case MOCHI_MOUTH_FLAT:
            /* Horizontal line */
            draw_line(layer, cx - SCALE(18), cy, cx + SCALE(18), cy, theme->mouth, lw + 1);
            break;

        case MOCHI_MOUTH_WAVY:
            /* Wavy line */
            draw_line(layer, cx - SCALE(20), cy, cx - SCALE(7), cy + SCALE(8), theme->mouth, lw);
            draw_line(layer, cx - SCALE(7), cy + SCALE(8), cx + SCALE(7), cy, theme->mouth, lw);
            draw_line(layer, cx + SCALE(7), cy, cx + SCALE(20), cy + SCALE(8), theme->mouth, lw);
            break;

        case MOCHI_MOUTH_SCREAM:
            /* Large O scream */
            draw_ellipse(layer, cx, cy + SCALE(5), SCALE(22), SCALE(25), theme->mouth, LV_OPA_COVER);
            break;

        default:
            /* Default to simple smile */
            draw_line(layer, cx - SCALE(15), cy, cx + SCALE(15), cy + SCALE(10), theme->mouth, lw);
            break;
    }
}

/*===========================================================================
 * Canvas Drawing Helper
 *===========================================================================*/

static void draw_face_to_canvas(void) {
    if (s_canvas == NULL || s_cached_theme == NULL) return;

    lv_layer_t layer;
    lv_canvas_init_layer(s_canvas, &layer);

    /* Draw all face elements */
    draw_background(&layer, s_cached_theme);
    draw_face(&layer, &s_cached_params, s_cached_theme);
    draw_blush(&layer, &s_cached_params, s_cached_theme);
    draw_eyes(&layer, &s_cached_params, s_cached_theme);
    draw_mouth(&layer, &s_cached_params, s_cached_theme);

    lv_canvas_finish_layer(s_canvas, &layer);
}

/*===========================================================================
 * Public Functions
 *===========================================================================*/

void mochi_face_create(lv_obj_t *parent) {
    if (s_canvas != NULL) {
        ESP_LOGW(TAG, "Face already created");
        return;
    }

    uint32_t bytes_needed = CANVAS_WIDTH * CANVAS_HEIGHT * 2;  /* RGB565 = 2 bytes/pixel */

    /* Log heap status before allocation */
    ESP_LOGI(TAG, "Creating mochi face canvas (%dx%d)", CANVAS_WIDTH, CANVAS_HEIGHT);
    ESP_LOGI(TAG, "Buffer needed: %lu bytes (%.1f KB)",
             (unsigned long)bytes_needed, bytes_needed / 1024.0f);
    ESP_LOGI(TAG, "Free heap: %lu bytes, largest block: %lu bytes",
             (unsigned long)esp_get_free_heap_size(),
             (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));

    /* Create draw buffer - RGB565 for efficiency (no transparency)
     * RGB565 = 2 bytes per pixel, ARGB8888 = 4 bytes per pixel */
    s_draw_buf = lv_draw_buf_create(CANVAS_WIDTH, CANVAS_HEIGHT, LV_COLOR_FORMAT_RGB565, 0);
    if (s_draw_buf == NULL) {
        ESP_LOGE(TAG, "Failed to create draw buffer (%lu bytes needed)",
                 (unsigned long)bytes_needed);
        ESP_LOGE(TAG, "Largest free block: %lu bytes - need contiguous memory!",
                 (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
        ESP_LOGE(TAG, "Try: reduce CANVAS_SIZE in mochi_face.c (currently %d)", CANVAS_SIZE);
        return;
    }

    /* Create canvas */
    s_canvas = lv_canvas_create(parent);
    lv_canvas_set_draw_buf(s_canvas, s_draw_buf);
    lv_obj_set_size(s_canvas, CANVAS_WIDTH, CANVAS_HEIGHT);
    lv_obj_center(s_canvas);

    /* Allow click events to pass through to parent */
    lv_obj_add_flag(s_canvas, LV_OBJ_FLAG_EVENT_BUBBLE);

    /* Initial fill - white */
    lv_canvas_fill_bg(s_canvas, lv_color_white(), LV_OPA_COVER);

    s_visible = true;
}

void mochi_face_destroy(void) {
    if (s_canvas == NULL) return;

    ESP_LOGI(TAG, "Destroying mochi face");

    lv_obj_delete(s_canvas);
    s_canvas = NULL;

    if (s_draw_buf != NULL) {
        lv_draw_buf_destroy(s_draw_buf);
        s_draw_buf = NULL;
    }

    s_cached_theme = NULL;
}

void mochi_face_update(const mochi_face_params_t *params, const mochi_theme_t *theme) {
    if (s_canvas == NULL || params == NULL || theme == NULL) return;

    /* Cache params for redraw */
    memcpy(&s_cached_params, params, sizeof(mochi_face_params_t));
    s_cached_theme = theme;

    /* Draw to canvas */
    draw_face_to_canvas();

    /* Invalidate to refresh display */
    lv_obj_invalidate(s_canvas);
}

void mochi_face_set_visible(bool visible) {
    if (s_canvas == NULL) return;

    s_visible = visible;
    if (visible) {
        lv_obj_remove_flag(s_canvas, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_canvas, LV_OBJ_FLAG_HIDDEN);
    }
}
