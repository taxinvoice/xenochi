/**
 * @file mochi_theme.h
 * @brief MochiState Theme Definitions - Color schemes for the mochi avatar
 *
 * 5 pastel color themes:
 * - Sakura: Pink/Rose
 * - Mint: Teal/Aqua
 * - Lavender: Purple
 * - Peach: Orange/Coral
 * - Cloud: Blue/Sky
 */
#pragma once

#include "lvgl.h"
#include "mochi_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Theme Color Structure
 *===========================================================================*/

/**
 * @brief Color palette for a mochi theme
 */
typedef struct {
    const char *name;           /**< Theme display name */
    lv_color_t bg;              /**< Background color */
    lv_color_t bg_light;        /**< Background gradient/highlight */
    lv_color_t face;            /**< Main face color */
    lv_color_t face_highlight;  /**< Face shine/highlight */
    lv_color_t face_shadow;     /**< Face shadow */
    lv_color_t eye;             /**< Eye background */
    lv_color_t pupil;           /**< Pupil color */
    lv_color_t mouth;           /**< Mouth color */
    lv_color_t blush;           /**< Blush circles */
    lv_color_t accent;          /**< Sparkles/particles */
    lv_color_t particle;        /**< Particle color */
} mochi_theme_t;

/*===========================================================================
 * Theme Definitions
 *===========================================================================*/

/**
 * @brief Sakura theme - Pink/Rose
 */
static const mochi_theme_t MOCHI_THEME_SAKURA_COLORS = {
    .name = "Sakura",
    .bg = LV_COLOR_MAKE(26, 22, 37),            /* #1a1625 */
    .bg_light = LV_COLOR_MAKE(45, 38, 64),      /* #2d2640 */
    .face = LV_COLOR_MAKE(255, 245, 245),       /* #fff5f5 */
    .face_highlight = LV_COLOR_MAKE(255, 255, 255), /* #ffffff */
    .face_shadow = LV_COLOR_MAKE(255, 228, 232),/* #ffe4e8 */
    .eye = LV_COLOR_MAKE(45, 38, 64),           /* #2d2640 */
    .pupil = LV_COLOR_MAKE(255, 107, 157),      /* #ff6b9d */
    .mouth = LV_COLOR_MAKE(255, 107, 157),      /* #ff6b9d */
    .blush = LV_COLOR_MAKE(255, 179, 198),      /* #ffb3c6 */
    .accent = LV_COLOR_MAKE(255, 155, 193),     /* #ff9bc1 */
    .particle = LV_COLOR_MAKE(255, 209, 220),   /* #ffd1dc */
};

/**
 * @brief Mint theme - Teal/Aqua
 */
static const mochi_theme_t MOCHI_THEME_MINT_COLORS = {
    .name = "Mint",
    .bg = LV_COLOR_MAKE(15, 26, 26),            /* #0f1a1a */
    .bg_light = LV_COLOR_MAKE(26, 47, 47),      /* #1a2f2f */
    .face = LV_COLOR_MAKE(240, 255, 255),       /* #f0ffff */
    .face_highlight = LV_COLOR_MAKE(255, 255, 255),
    .face_shadow = LV_COLOR_MAKE(212, 245, 245),/* #d4f5f5 */
    .eye = LV_COLOR_MAKE(26, 47, 47),           /* #1a2f2f */
    .pupil = LV_COLOR_MAKE(64, 201, 198),       /* #40c9c6 */
    .mouth = LV_COLOR_MAKE(64, 201, 198),       /* #40c9c6 */
    .blush = LV_COLOR_MAKE(168, 230, 207),      /* #a8e6cf */
    .accent = LV_COLOR_MAKE(127, 219, 218),     /* #7fdbda */
    .particle = LV_COLOR_MAKE(200, 247, 247),   /* #c8f7f7 */
};

/**
 * @brief Lavender theme - Purple
 */
static const mochi_theme_t MOCHI_THEME_LAVENDER_COLORS = {
    .name = "Lavender",
    .bg = LV_COLOR_MAKE(26, 22, 37),            /* #1a1625 */
    .bg_light = LV_COLOR_MAKE(42, 32, 64),      /* #2a2040 */
    .face = LV_COLOR_MAKE(248, 245, 255),       /* #f8f5ff */
    .face_highlight = LV_COLOR_MAKE(255, 255, 255),
    .face_shadow = LV_COLOR_MAKE(232, 224, 240),/* #e8e0f0 */
    .eye = LV_COLOR_MAKE(42, 32, 64),           /* #2a2040 */
    .pupil = LV_COLOR_MAKE(157, 124, 216),      /* #9d7cd8 */
    .mouth = LV_COLOR_MAKE(157, 124, 216),      /* #9d7cd8 */
    .blush = LV_COLOR_MAKE(219, 184, 255),      /* #dbb8ff */
    .accent = LV_COLOR_MAKE(196, 167, 231),     /* #c4a7e7 */
    .particle = LV_COLOR_MAKE(232, 213, 255),   /* #e8d5ff */
};

/**
 * @brief Peach theme - Orange/Coral
 */
static const mochi_theme_t MOCHI_THEME_PEACH_COLORS = {
    .name = "Peach",
    .bg = LV_COLOR_MAKE(31, 23, 20),            /* #1f1714 */
    .bg_light = LV_COLOR_MAKE(45, 36, 32),      /* #2d2420 */
    .face = LV_COLOR_MAKE(255, 248, 240),       /* #fff8f0 */
    .face_highlight = LV_COLOR_MAKE(255, 255, 255),
    .face_shadow = LV_COLOR_MAKE(255, 232, 214),/* #ffe8d6 */
    .eye = LV_COLOR_MAKE(45, 36, 32),           /* #2d2420 */
    .pupil = LV_COLOR_MAKE(255, 140, 90),       /* #ff8c5a */
    .mouth = LV_COLOR_MAKE(255, 140, 90),       /* #ff8c5a */
    .blush = LV_COLOR_MAKE(255, 196, 168),      /* #ffc4a8 */
    .accent = LV_COLOR_MAKE(255, 176, 136),     /* #ffb088 */
    .particle = LV_COLOR_MAKE(255, 216, 200),   /* #ffd8c8 */
};

/**
 * @brief Cloud theme - Blue/Sky
 */
static const mochi_theme_t MOCHI_THEME_CLOUD_COLORS = {
    .name = "Cloud",
    .bg = LV_COLOR_MAKE(21, 24, 32),            /* #151820 */
    .bg_light = LV_COLOR_MAKE(32, 37, 53),      /* #202535 */
    .face = LV_COLOR_MAKE(245, 248, 255),       /* #f5f8ff */
    .face_highlight = LV_COLOR_MAKE(255, 255, 255),
    .face_shadow = LV_COLOR_MAKE(221, 229, 245),/* #dde5f5 */
    .eye = LV_COLOR_MAKE(32, 37, 53),           /* #202535 */
    .pupil = LV_COLOR_MAKE(85, 136, 204),       /* #5588cc */
    .mouth = LV_COLOR_MAKE(85, 136, 204),       /* #5588cc */
    .blush = LV_COLOR_MAKE(184, 208, 240),      /* #b8d0f0 */
    .accent = LV_COLOR_MAKE(136, 168, 216),     /* #88a8d8 */
    .particle = LV_COLOR_MAKE(208, 224, 255),   /* #d0e0ff */
};

/*===========================================================================
 * Theme Array for Runtime Access
 *===========================================================================*/

/**
 * @brief Array of all theme pointers for easy access by ID
 */
static const mochi_theme_t* const MOCHI_THEMES[MOCHI_THEME_MAX] = {
    &MOCHI_THEME_SAKURA_COLORS,
    &MOCHI_THEME_MINT_COLORS,
    &MOCHI_THEME_LAVENDER_COLORS,
    &MOCHI_THEME_PEACH_COLORS,
    &MOCHI_THEME_CLOUD_COLORS,
};

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Get theme colors by ID
 *
 * @param theme_id Theme ID
 * @return Pointer to theme colors, or Sakura if invalid
 */
static inline const mochi_theme_t* mochi_get_theme_colors(mochi_theme_id_t theme_id) {
    if (theme_id >= MOCHI_THEME_MAX) {
        return &MOCHI_THEME_SAKURA_COLORS;
    }
    return MOCHI_THEMES[theme_id];
}

#ifdef __cplusplus
}
#endif
