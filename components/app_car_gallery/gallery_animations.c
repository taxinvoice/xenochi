/**
 * @file gallery_animations.c
 * @brief Car Animation Gallery - Non-face animation implementations
 *
 * 36 creative animations rendered using LVGL direct layer drawing.
 * Each animation has its own draw function called at 20 FPS.
 */

#include "gallery_animations.h"
#include "esp_log.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "GalleryAnim";

/*===========================================================================
 * Constants
 *===========================================================================*/

#define SCREEN_WIDTH    240
#define SCREEN_HEIGHT   284
#define CENTER_X        (SCREEN_WIDTH / 2)
#define CENTER_Y        (SCREEN_HEIGHT / 2)
#define ANIM_FPS        20
#define ANIM_PERIOD_MS  (1000 / ANIM_FPS)

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/*===========================================================================
 * Animation Metadata
 *===========================================================================*/

static const gallery_anim_info_t s_anim_info[GALLERY_ANIM_MAX] = {
    /* Abstract Geometric */
    {"Pulsing Rings", "Concentric expanding circles", 0x00BCD4, 0xFFFFFF},
    {"Spiral Galaxy", "Rotating spiral pattern", 0x9C27B0, 0x3F51B5},
    {"Heartbeat", "EKG pulse line", 0x4CAF50, 0x1B5E20},
    {"Breathing Orb", "Expanding/contracting circle", 0x03A9F4, 0x01579B},
    {"Matrix Rain", "Falling digital rain", 0x00FF00, 0x003300},
    {"Radar Sweep", "Rotating radar scan", 0x00FF00, 0x004400},

    /* Weather Effects */
    {"Rain Storm", "Falling raindrops", 0x2196F3, 0xFFFFFF},
    {"Snowfall", "Drifting snowflakes", 0xFFFFFF, 0x1565C0},
    {"Sunshine", "Radiating sun rays", 0xFFEB3B, 0xFF9800},
    {"Lightning", "Flash and bolt", 0xFFFFFF, 0x7C4DFF},
    {"Starry Night", "Twinkling stars", 0xFFFFFF, 0x1A237E},
    {"Aurora", "Northern lights waves", 0x00E676, 0xE040FB},

    /* Emoji/Symbols */
    {"Hearts", "Floating hearts", 0xE91E63, 0xF48FB1},
    {"Star Burst", "Exploding stars", 0xFFD700, 0xFFA000},
    {"Question", "Bouncing ? symbol", 0x2196F3, 0xFFFFFF},
    {"Exclamation", "Pulsing ! warning", 0xF44336, 0xFF9800},
    {"Checkmark", "Green check animation", 0x4CAF50, 0xFFFFFF},
    {"X Mark", "Red X with shake", 0xF44336, 0xB71C1C},

    /* Tech/Digital */
    {"Loading", "Spinning dots", 0xFFFFFF, 0x757575},
    {"Progress", "Filling bar", 0x2196F3, 0xFFFFFF},
    {"Sound Waves", "Audio visualizer", 0x4CAF50, 0x1B5E20},
    {"WiFi Signal", "Animated arcs", 0xFFFFFF, 0x2196F3},
    {"Battery", "Charging animation", 0x4CAF50, 0xFFEB3B},
    {"Binary", "Scrolling 0s and 1s", 0x00FF00, 0x001100},

    /* Nature/Organic */
    {"Bouncing Ball", "Physics bounce", 0xF44336, 0x424242},
    {"Ocean Waves", "Rolling sine waves", 0x1565C0, 0x42A5F5},
    {"Butterfly", "Flapping wings", 0xFF9800, 0xFFEB3B},
    {"Fireworks", "Burst explosions", 0xFF5722, 0xFFEB3B},
    {"Campfire", "Flickering flames", 0xFF5722, 0xFFEB3B},
    {"Bubbles", "Rising and popping", 0x81D4FA, 0xFFFFFF},

    /* Dashboard/Automotive */
    {"Speedometer", "Sweeping gauge", 0xFFFFFF, 0xF44336},
    {"Fuel Gauge", "Needle oscillation", 0xFFFFFF, 0xFF9800},
    {"Turn Left", "Blinking arrow", 0x4CAF50, 0x1B5E20},
    {"Turn Right", "Blinking arrow", 0x4CAF50, 0x1B5E20},
    {"Hazard", "Both arrows blink", 0xFF9800, 0xE65100},
    {"Gear", "Shift display", 0xFFFFFF, 0x2196F3},
};

/*===========================================================================
 * State Variables
 *===========================================================================*/

static lv_obj_t *s_draw_obj = NULL;
static lv_timer_t *s_anim_timer = NULL;
static gallery_anim_id_t s_current_anim = GALLERY_ANIM_PULSING_RINGS;
static float s_time = 0.0f;
static bool s_visible = false;

/* Animation-specific state */
static struct {
    /* Rain/Snow particles */
    struct { float x, y, speed; } particles[40];
    int particle_count;

    /* Stars */
    struct { float x, y, phase, speed; } stars[40];

    /* Fireworks */
    struct { float x, y, vx, vy, life; } sparks[50];
    float launch_x, launch_y, launch_vy;
    int spark_count;
    bool launching;

    /* Sound waves */
    float bar_heights[12];
    float bar_targets[12];

    /* Bouncing ball */
    float ball_y, ball_vy;

    /* Binary code */
    char binary_cols[6][10];
    float scroll_offset;

    /* Gear display */
    int current_gear;
    float gear_timer;

} s_state;

/*===========================================================================
 * Drawing Helpers (same as mochi_face.c)
 *===========================================================================*/

static void draw_filled_rect(lv_layer_t *layer, int x, int y, int w, int h,
                             lv_color_t color, lv_opa_t opa)
{
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = opa;
    dsc.radius = 0;
    dsc.border_width = 0;

    lv_area_t area = { x, y, x + w - 1, y + h - 1 };
    lv_draw_rect(layer, &dsc, &area);
}

static void draw_circle(lv_layer_t *layer, int cx, int cy, int r,
                        lv_color_t color, lv_opa_t opa)
{
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = opa;
    dsc.radius = LV_RADIUS_CIRCLE;
    dsc.border_width = 0;

    lv_area_t area = { cx - r, cy - r, cx + r, cy + r };
    lv_draw_rect(layer, &dsc, &area);
}

static void draw_ring(lv_layer_t *layer, int cx, int cy, int r, int thickness,
                      lv_color_t color, lv_opa_t opa)
{
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.color = color;
    dsc.opa = opa;
    dsc.width = thickness;
    dsc.center.x = cx;
    dsc.center.y = cy;
    dsc.start_angle = 0;
    dsc.end_angle = 360;
    dsc.radius = r;

    lv_draw_arc(layer, &dsc);
}

static void draw_line(lv_layer_t *layer, int x1, int y1, int x2, int y2,
                      lv_color_t color, int width)
{
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = color;
    dsc.width = width;
    dsc.round_start = true;
    dsc.round_end = true;
    dsc.opa = LV_OPA_COVER;
    dsc.p1.x = x1;
    dsc.p1.y = y1;
    dsc.p2.x = x2;
    dsc.p2.y = y2;

    lv_draw_line(layer, &dsc);
}

static void draw_arc(lv_layer_t *layer, int cx, int cy, int r,
                     int start_angle, int end_angle, int width,
                     lv_color_t color, lv_opa_t opa)
{
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.color = color;
    dsc.opa = opa;
    dsc.width = width;
    dsc.center.x = cx;
    dsc.center.y = cy;
    dsc.start_angle = start_angle;
    dsc.end_angle = end_angle;
    dsc.radius = r;

    lv_draw_arc(layer, &dsc);
}

static void fill_background(lv_layer_t *layer, uint32_t color_hex)
{
    draw_filled_rect(layer, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                     lv_color_hex(color_hex), LV_OPA_COVER);
}

/*===========================================================================
 * Abstract Geometric Animations
 *===========================================================================*/

static void draw_pulsing_rings(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x0D1B2A);

    uint32_t colors[] = {0x00BCD4, 0x00ACC1, 0x0097A7, 0x00838F};

    for (int i = 0; i < 4; i++) {
        float phase = fmodf(t + i * 0.25f, 1.0f);
        int r = (int)(30 + phase * 100);
        lv_opa_t opa = (lv_opa_t)((1.0f - phase) * 200);

        draw_ring(layer, CENTER_X, CENTER_Y, r, 4,
                  lv_color_hex(colors[i]), opa);
    }
}

static void draw_spiral_galaxy(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x0D0221);

    float rotation = t * 0.5f * M_PI;

    for (int arm = 0; arm < 3; arm++) {
        float arm_offset = arm * (2.0f * M_PI / 3.0f);

        for (int i = 0; i < 30; i++) {
            float angle = arm_offset + rotation + (i * 0.15f);
            float radius = 10 + i * 3;

            int x = CENTER_X + (int)(cosf(angle) * radius);
            int y = CENTER_Y + (int)(sinf(angle) * radius);

            uint8_t brightness = 255 - (i * 6);
            draw_circle(layer, x, y, 3,
                       lv_color_make(brightness/2, brightness/3, brightness),
                       LV_OPA_COVER);
        }
    }

    /* Center glow */
    draw_circle(layer, CENTER_X, CENTER_Y, 15, lv_color_hex(0xE1BEE7), LV_OPA_80);
    draw_circle(layer, CENTER_X, CENTER_Y, 8, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
}

static void draw_heartbeat(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x001100);

    /* Draw grid lines */
    for (int x = 0; x < SCREEN_WIDTH; x += 20) {
        draw_line(layer, x, 0, x, SCREEN_HEIGHT, lv_color_hex(0x003300), 1);
    }
    for (int y = 0; y < SCREEN_HEIGHT; y += 20) {
        draw_line(layer, 0, y, SCREEN_WIDTH, y, lv_color_hex(0x003300), 1);
    }

    /* EKG line */
    int prev_y = CENTER_Y;
    float sweep = fmodf(t * 0.5f, 1.0f) * SCREEN_WIDTH;

    for (int x = 0; x < SCREEN_WIDTH - 1; x++) {
        float phase = fmodf((float)x / SCREEN_WIDTH + t * 0.5f, 1.0f);
        int y = CENTER_Y;

        /* Create heartbeat spike pattern */
        if (phase > 0.4f && phase < 0.45f) {
            y = CENTER_Y - 40;  /* Up spike */
        } else if (phase > 0.45f && phase < 0.5f) {
            y = CENTER_Y + 60;  /* Down spike */
        } else if (phase > 0.5f && phase < 0.55f) {
            y = CENTER_Y - 20;  /* Recovery */
        }

        /* Fade near sweep line */
        lv_opa_t opa = LV_OPA_COVER;
        float dist_to_sweep = fabsf((float)x - sweep);
        if (dist_to_sweep < 30) {
            opa = (lv_opa_t)(255 * (dist_to_sweep / 30));
        }

        if (x > 0) {
            draw_line(layer, x - 1, prev_y, x, y, lv_color_hex(0x00FF00), 2);
        }
        prev_y = y;
    }

    /* Sweep line */
    draw_line(layer, (int)sweep, 0, (int)sweep, SCREEN_HEIGHT,
              lv_color_hex(0x00FF00), 1);
}

static void draw_breathing_orb(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x01579B);

    float scale = 0.7f + 0.3f * sinf(t * 2.0f * M_PI * 0.2f);
    int base_r = 60;

    /* Outer glow layers */
    for (int i = 4; i >= 0; i--) {
        int r = (int)(base_r * scale) + i * 15;
        lv_opa_t opa = 30 + (4 - i) * 20;
        draw_circle(layer, CENTER_X, CENTER_Y, r, lv_color_hex(0x03A9F4), opa);
    }

    /* Core */
    draw_circle(layer, CENTER_X, CENTER_Y, (int)(base_r * scale),
                lv_color_hex(0x4FC3F7), LV_OPA_COVER);
    draw_circle(layer, CENTER_X, CENTER_Y, (int)(base_r * scale * 0.6f),
                lv_color_hex(0xB3E5FC), LV_OPA_COVER);
}

static void draw_matrix_rain(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x000800);

    /* Initialize particles if needed */
    if (s_state.particle_count == 0) {
        s_state.particle_count = 20;
        for (int i = 0; i < 20; i++) {
            s_state.particles[i].x = (rand() % SCREEN_WIDTH);
            s_state.particles[i].y = -(rand() % SCREEN_HEIGHT);
            s_state.particles[i].speed = 100 + (rand() % 150);
        }
    }

    /* Draw falling streams */
    for (int i = 0; i < 20; i++) {
        /* Update position */
        s_state.particles[i].y += s_state.particles[i].speed * (1.0f / ANIM_FPS);

        if (s_state.particles[i].y > SCREEN_HEIGHT + 100) {
            s_state.particles[i].y = -(rand() % 100);
            s_state.particles[i].x = (rand() % SCREEN_WIDTH);
        }

        /* Draw trail */
        int x = (int)s_state.particles[i].x;
        for (int j = 0; j < 10; j++) {
            int y = (int)s_state.particles[i].y - j * 12;
            if (y >= 0 && y < SCREEN_HEIGHT) {
                lv_opa_t opa = 255 - j * 25;
                uint8_t green = 255 - j * 15;
                draw_filled_rect(layer, x, y, 8, 10,
                                lv_color_make(0, green, 0), opa);
            }
        }
    }
}

static void draw_radar_sweep(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x001a00);

    /* Draw concentric rings */
    for (int r = 30; r <= 120; r += 30) {
        draw_ring(layer, CENTER_X, CENTER_Y, r, 1, lv_color_hex(0x004400), LV_OPA_COVER);
    }

    /* Draw cross hairs */
    draw_line(layer, CENTER_X - 120, CENTER_Y, CENTER_X + 120, CENTER_Y,
              lv_color_hex(0x004400), 1);
    draw_line(layer, CENTER_X, CENTER_Y - 120, CENTER_X, CENTER_Y + 120,
              lv_color_hex(0x004400), 1);

    /* Sweep line */
    float angle = fmodf(t * M_PI, 2.0f * M_PI);
    int sweep_x = CENTER_X + (int)(cosf(angle) * 120);
    int sweep_y = CENTER_Y + (int)(sinf(angle) * 120);

    /* Fade trail */
    for (int i = 0; i < 30; i++) {
        float trail_angle = angle - (i * 0.03f);
        int tx = CENTER_X + (int)(cosf(trail_angle) * 120);
        int ty = CENTER_Y + (int)(sinf(trail_angle) * 120);
        lv_opa_t opa = 255 - i * 8;
        draw_line(layer, CENTER_X, CENTER_Y, tx, ty, lv_color_hex(0x00FF00), 1);
    }

    /* Main sweep line */
    draw_line(layer, CENTER_X, CENTER_Y, sweep_x, sweep_y,
              lv_color_hex(0x00FF00), 2);

    /* Random blips */
    int blip_seed = (int)(t * 3) % 5;
    for (int i = 0; i < 3; i++) {
        int blip_angle = (blip_seed + i * 37) % 360;
        int blip_r = 30 + ((blip_seed + i * 17) % 90);
        int bx = CENTER_X + (int)(cosf(blip_angle * M_PI / 180) * blip_r);
        int by = CENTER_Y + (int)(sinf(blip_angle * M_PI / 180) * blip_r);

        float age = fmodf(t + i * 0.5f, 2.0f);
        if (age < 1.0f) {
            lv_opa_t opa = (lv_opa_t)((1.0f - age) * 255);
            draw_circle(layer, bx, by, 4, lv_color_hex(0x00FF00), opa);
        }
    }

    /* Center dot */
    draw_circle(layer, CENTER_X, CENTER_Y, 3, lv_color_hex(0x00FF00), LV_OPA_COVER);
}

/*===========================================================================
 * Weather Effect Animations
 *===========================================================================*/

static void draw_rain_storm(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x1A237E);

    /* Initialize drops */
    if (s_state.particle_count == 0) {
        s_state.particle_count = 30;
        for (int i = 0; i < 30; i++) {
            s_state.particles[i].x = rand() % SCREEN_WIDTH;
            s_state.particles[i].y = rand() % SCREEN_HEIGHT;
            s_state.particles[i].speed = 300 + (rand() % 200);
        }
    }

    for (int i = 0; i < 30; i++) {
        s_state.particles[i].y += s_state.particles[i].speed * (1.0f / ANIM_FPS);

        if (s_state.particles[i].y > SCREEN_HEIGHT) {
            /* Splash effect - draw circle at bottom */
            int splash_x = (int)s_state.particles[i].x;
            draw_circle(layer, splash_x, SCREEN_HEIGHT - 5, 3,
                       lv_color_hex(0xBBDEFB), LV_OPA_50);

            /* Reset drop */
            s_state.particles[i].y = -(rand() % 50);
            s_state.particles[i].x = rand() % SCREEN_WIDTH;
        }

        /* Draw raindrop as short line */
        int x = (int)s_state.particles[i].x;
        int y = (int)s_state.particles[i].y;
        draw_line(layer, x, y, x, y + 15, lv_color_hex(0x90CAF9), 2);
    }
}

static void draw_snowfall(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x1A237E);

    /* Initialize flakes */
    if (s_state.particle_count == 0) {
        s_state.particle_count = 25;
        for (int i = 0; i < 25; i++) {
            s_state.particles[i].x = rand() % SCREEN_WIDTH;
            s_state.particles[i].y = rand() % SCREEN_HEIGHT;
            s_state.particles[i].speed = 30 + (rand() % 40);
        }
    }

    for (int i = 0; i < 25; i++) {
        /* Drift with sine wave */
        s_state.particles[i].x += sinf(t * 2 + i) * 0.5f;
        s_state.particles[i].y += s_state.particles[i].speed * (1.0f / ANIM_FPS);

        if (s_state.particles[i].y > SCREEN_HEIGHT) {
            s_state.particles[i].y = -10;
            s_state.particles[i].x = rand() % SCREEN_WIDTH;
        }
        if (s_state.particles[i].x < 0) s_state.particles[i].x = SCREEN_WIDTH;
        if (s_state.particles[i].x > SCREEN_WIDTH) s_state.particles[i].x = 0;

        int r = 2 + (i % 4);
        draw_circle(layer, (int)s_state.particles[i].x, (int)s_state.particles[i].y,
                   r, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
    }

    /* Ground snow accumulation */
    draw_filled_rect(layer, 0, SCREEN_HEIGHT - 20, SCREEN_WIDTH, 20,
                    lv_color_hex(0xE8EAF6), LV_OPA_COVER);
}

static void draw_sunshine(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x87CEEB);

    float rotation = t * 0.5f;

    /* Draw rays */
    for (int i = 0; i < 12; i++) {
        float angle = rotation + i * (M_PI / 6);
        int x1 = CENTER_X + (int)(cosf(angle) * 50);
        int y1 = CENTER_Y + (int)(sinf(angle) * 50);
        int x2 = CENTER_X + (int)(cosf(angle) * 100);
        int y2 = CENTER_Y + (int)(sinf(angle) * 100);

        draw_line(layer, x1, y1, x2, y2, lv_color_hex(0xFFD54F), 4);
    }

    /* Sun body with glow */
    draw_circle(layer, CENTER_X, CENTER_Y, 55, lv_color_hex(0xFFE082), LV_OPA_50);
    draw_circle(layer, CENTER_X, CENTER_Y, 45, lv_color_hex(0xFFD54F), LV_OPA_COVER);
    draw_circle(layer, CENTER_X, CENTER_Y, 38, lv_color_hex(0xFFEB3B), LV_OPA_COVER);
}

static void draw_lightning(lv_layer_t *layer, float t)
{
    /* Determine if flash is happening */
    float cycle = fmodf(t, 3.0f);
    bool flash = (cycle < 0.1f) || (cycle > 0.15f && cycle < 0.2f);

    if (flash) {
        fill_background(layer, 0xFFFFFF);

        /* Draw bolt */
        int points[][2] = {
            {CENTER_X, 20},
            {CENTER_X - 20, 80},
            {CENTER_X + 10, 80},
            {CENTER_X - 30, 160},
            {CENTER_X + 20, 160},
            {CENTER_X - 10, SCREEN_HEIGHT - 40}
        };

        for (int i = 0; i < 5; i++) {
            draw_line(layer, points[i][0], points[i][1],
                     points[i+1][0], points[i+1][1],
                     lv_color_hex(0xFFEB3B), 4);
        }
    } else {
        /* Dark stormy sky */
        fill_background(layer, 0x1A237E);

        /* Clouds */
        draw_circle(layer, 60, 60, 40, lv_color_hex(0x37474F), LV_OPA_COVER);
        draw_circle(layer, 100, 50, 35, lv_color_hex(0x455A64), LV_OPA_COVER);
        draw_circle(layer, 180, 70, 45, lv_color_hex(0x37474F), LV_OPA_COVER);
        draw_circle(layer, 140, 55, 30, lv_color_hex(0x546E7A), LV_OPA_COVER);
    }
}

static void draw_starry_night(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x0D1B2A);

    /* Initialize stars */
    static bool initialized = false;
    if (!initialized) {
        for (int i = 0; i < 40; i++) {
            s_state.stars[i].x = rand() % SCREEN_WIDTH;
            s_state.stars[i].y = rand() % SCREEN_HEIGHT;
            s_state.stars[i].phase = (float)(rand() % 100) / 100.0f * 2.0f * M_PI;
            s_state.stars[i].speed = 1.0f + (float)(rand() % 30) / 10.0f;
        }
        initialized = true;
    }

    for (int i = 0; i < 40; i++) {
        float twinkle = 0.5f + 0.5f * sinf(t * s_state.stars[i].speed + s_state.stars[i].phase);
        lv_opa_t opa = (lv_opa_t)(twinkle * 255);

        int r = (i % 3) + 1;
        lv_color_t color = (i % 5 == 0) ? lv_color_hex(0xFFEB3B) : lv_color_hex(0xFFFFFF);

        draw_circle(layer, (int)s_state.stars[i].x, (int)s_state.stars[i].y,
                   r, color, opa);
    }

    /* Moon */
    draw_circle(layer, 50, 60, 25, lv_color_hex(0xFFFDE7), LV_OPA_COVER);
    draw_circle(layer, 40, 55, 22, lv_color_hex(0x0D1B2A), LV_OPA_COVER);
}

static void draw_aurora(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x0D1B2A);

    uint32_t colors[] = {0x00E676, 0x00BCD4, 0xE040FB, 0xEC407A, 0x7C4DFF};

    for (int band = 0; band < 5; band++) {
        for (int x = 0; x < SCREEN_WIDTH; x += 2) {
            float wave = sinf(x * 0.03f + t * 0.5f + band * 0.5f) * 20;
            int y = 60 + band * 35 + (int)wave;

            lv_opa_t opa = 80 + (int)(40 * sinf(x * 0.05f + t));
            draw_filled_rect(layer, x, y, 3, 25, lv_color_hex(colors[band]), opa);
        }
    }
}

/*===========================================================================
 * Emoji/Symbol Animations
 *===========================================================================*/

static void draw_heart_shape(lv_layer_t *layer, int cx, int cy, int size,
                             lv_color_t color, lv_opa_t opa)
{
    /* Heart = two circles + triangle */
    int r = size / 3;
    draw_circle(layer, cx - r, cy - r/2, r, color, opa);
    draw_circle(layer, cx + r, cy - r/2, r, color, opa);

    /* Bottom triangle using filled rectangles */
    for (int y = 0; y < size; y++) {
        int width = size - y;
        if (width > 0) {
            draw_filled_rect(layer, cx - width/2, cy + y, width, 1, color, opa);
        }
    }
}

static void draw_floating_hearts(lv_layer_t *layer, float t)
{
    fill_background(layer, 0xFCE4EC);

    for (int i = 0; i < 8; i++) {
        float phase = fmodf(t * 0.5f + i * 0.2f, 1.0f);
        int x = 30 + (i * 30);
        int y = (int)(SCREEN_HEIGHT - phase * (SCREEN_HEIGHT + 50));

        float wobble = sinf(t * 3 + i) * 10;
        x += (int)wobble;

        int size = 15 + (i % 3) * 5;
        lv_color_t color = (i % 2) ? lv_color_hex(0xE91E63) : lv_color_hex(0xF48FB1);

        draw_heart_shape(layer, x, y, size, color, LV_OPA_COVER);
    }
}

static void draw_star_shape(lv_layer_t *layer, int cx, int cy, int outer_r, int inner_r,
                            lv_color_t color, lv_opa_t opa)
{
    /* 5-pointed star using lines */
    for (int i = 0; i < 5; i++) {
        float angle1 = -M_PI/2 + i * (2 * M_PI / 5);
        float angle2 = angle1 + M_PI / 5;

        int x1 = cx + (int)(cosf(angle1) * outer_r);
        int y1 = cy + (int)(sinf(angle1) * outer_r);
        int x2 = cx + (int)(cosf(angle2) * inner_r);
        int y2 = cy + (int)(sinf(angle2) * inner_r);
        int x3 = cx + (int)(cosf(angle1 + 2*M_PI/5) * outer_r);
        int y3 = cy + (int)(sinf(angle1 + 2*M_PI/5) * outer_r);

        draw_line(layer, x1, y1, x2, y2, color, 2);
        draw_line(layer, x2, y2, x3, y3, color, 2);
    }
}

static void draw_star_burst(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x1A237E);

    float cycle = fmodf(t, 2.0f);

    for (int i = 0; i < 8; i++) {
        float angle = i * (M_PI / 4);
        float dist = cycle * 80;

        int x = CENTER_X + (int)(cosf(angle) * dist);
        int y = CENTER_Y + (int)(sinf(angle) * dist);

        lv_opa_t opa = (lv_opa_t)((1.0f - cycle / 2.0f) * 255);
        int size = 10 + (int)(cycle * 10);

        draw_star_shape(layer, x, y, size, size/2, lv_color_hex(0xFFD700), opa);
    }

    /* Center star */
    draw_star_shape(layer, CENTER_X, CENTER_Y, 25, 12, lv_color_hex(0xFFEB3B), LV_OPA_COVER);
}

static void draw_question_mark(lv_layer_t *layer, float t)
{
    fill_background(layer, 0xE3F2FD);

    float bounce = fabsf(sinf(t * 3)) * 20;
    float wobble = sinf(t * 2) * 5;

    int cx = CENTER_X + (int)wobble;
    int cy = CENTER_Y - (int)bounce;

    /* Question mark curve */
    draw_arc(layer, cx, cy - 20, 25, 200, 360, 8, lv_color_hex(0x2196F3), LV_OPA_COVER);
    draw_arc(layer, cx, cy - 20, 25, 0, 90, 8, lv_color_hex(0x2196F3), LV_OPA_COVER);

    /* Vertical part */
    draw_filled_rect(layer, cx - 4, cy, 8, 25, lv_color_hex(0x2196F3), LV_OPA_COVER);

    /* Dot */
    draw_circle(layer, cx, cy + 45, 6, lv_color_hex(0x2196F3), LV_OPA_COVER);
}

static void draw_exclamation(lv_layer_t *layer, float t)
{
    fill_background(layer, 0xFFEBEE);

    float pulse = 1.0f + 0.3f * sinf(t * 8);
    float shake = sinf(t * 20) * 3;

    int cx = CENTER_X + (int)shake;

    /* Exclamation body */
    int h = (int)(60 * pulse);
    draw_filled_rect(layer, cx - 6, CENTER_Y - h/2 - 20, 12, h,
                    lv_color_hex(0xF44336), LV_OPA_COVER);

    /* Dot */
    int dot_r = (int)(8 * pulse);
    draw_circle(layer, cx, CENTER_Y + 40, dot_r, lv_color_hex(0xF44336), LV_OPA_COVER);
}

static void draw_checkmark(lv_layer_t *layer, float t)
{
    fill_background(layer, 0xE8F5E9);

    /* Draw-in animation */
    float progress = fminf(fmodf(t, 2.0f), 1.0f);

    /* Circle background */
    draw_circle(layer, CENTER_X, CENTER_Y, 60, lv_color_hex(0x4CAF50), LV_OPA_COVER);

    /* Checkmark */
    if (progress > 0) {
        int x1 = CENTER_X - 30, y1 = CENTER_Y;
        int x2 = CENTER_X - 10, y2 = CENTER_Y + 25;
        int x3 = CENTER_X + 30, y3 = CENTER_Y - 25;

        if (progress < 0.5f) {
            /* Draw first part */
            float p = progress * 2;
            int ex = x1 + (int)((x2 - x1) * p);
            int ey = y1 + (int)((y2 - y1) * p);
            draw_line(layer, x1, y1, ex, ey, lv_color_hex(0xFFFFFF), 8);
        } else {
            /* Draw full first part and partial second */
            draw_line(layer, x1, y1, x2, y2, lv_color_hex(0xFFFFFF), 8);
            float p = (progress - 0.5f) * 2;
            int ex = x2 + (int)((x3 - x2) * p);
            int ey = y2 + (int)((y3 - y2) * p);
            draw_line(layer, x2, y2, ex, ey, lv_color_hex(0xFFFFFF), 8);
        }
    }

    /* Sparkles after completion */
    if (progress >= 1.0f) {
        for (int i = 0; i < 6; i++) {
            float angle = i * M_PI / 3 + t;
            int sx = CENTER_X + (int)(cosf(angle) * 80);
            int sy = CENTER_Y + (int)(sinf(angle) * 80);
            draw_circle(layer, sx, sy, 4, lv_color_hex(0xFFFFFF), LV_OPA_80);
        }
    }
}

static void draw_x_mark(lv_layer_t *layer, float t)
{
    fill_background(layer, 0xFFEBEE);

    float shake = sinf(t * 15) * 5;
    int cx = CENTER_X + (int)shake;

    /* Circle background */
    draw_circle(layer, cx, CENTER_Y, 60, lv_color_hex(0xF44336), LV_OPA_COVER);

    /* X mark */
    int size = 35;
    draw_line(layer, cx - size, CENTER_Y - size, cx + size, CENTER_Y + size,
              lv_color_hex(0xFFFFFF), 8);
    draw_line(layer, cx - size, CENTER_Y + size, cx + size, CENTER_Y - size,
              lv_color_hex(0xFFFFFF), 8);
}

/*===========================================================================
 * Tech/Digital Animations
 *===========================================================================*/

static void draw_loading_spinner(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x263238);

    int active = ((int)(t * 8)) % 8;

    for (int i = 0; i < 8; i++) {
        float angle = i * (M_PI / 4) - M_PI/2;
        int x = CENTER_X + (int)(cosf(angle) * 50);
        int y = CENTER_Y + (int)(sinf(angle) * 50);

        lv_opa_t opa = (i == active) ? LV_OPA_COVER : LV_OPA_30;
        lv_color_t color = (i == active) ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x78909C);

        draw_circle(layer, x, y, 10, color, opa);
    }
}

static void draw_progress_bar(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x37474F);

    float progress = fmodf(t * 0.3f, 1.0f);

    /* Bar outline */
    int bar_x = 30, bar_y = CENTER_Y - 15;
    int bar_w = SCREEN_WIDTH - 60, bar_h = 30;

    draw_filled_rect(layer, bar_x - 2, bar_y - 2, bar_w + 4, bar_h + 4,
                    lv_color_hex(0xFFFFFF), LV_OPA_COVER);
    draw_filled_rect(layer, bar_x, bar_y, bar_w, bar_h,
                    lv_color_hex(0x263238), LV_OPA_COVER);

    /* Fill */
    int fill_w = (int)(bar_w * progress);
    draw_filled_rect(layer, bar_x, bar_y, fill_w, bar_h,
                    lv_color_hex(0x2196F3), LV_OPA_COVER);

    /* Percentage text (simplified - just draw markers) */
    for (int i = 1; i < 4; i++) {
        int mark_x = bar_x + (bar_w * i / 4);
        draw_line(layer, mark_x, bar_y + bar_h + 5, mark_x, bar_y + bar_h + 10,
                 lv_color_hex(0xFFFFFF), 1);
    }
}

static void draw_sound_waves(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x1B5E20);

    /* Update bar targets */
    for (int i = 0; i < 12; i++) {
        if (rand() % 5 == 0) {
            s_state.bar_targets[i] = 30 + (rand() % 100);
        }
        /* Smooth interpolation */
        s_state.bar_heights[i] += (s_state.bar_targets[i] - s_state.bar_heights[i]) * 0.2f;
    }

    /* Draw bars */
    int bar_w = 15;
    int gap = 5;
    int start_x = (SCREEN_WIDTH - 12 * (bar_w + gap)) / 2;

    for (int i = 0; i < 12; i++) {
        int h = (int)s_state.bar_heights[i];
        int x = start_x + i * (bar_w + gap);
        int y = SCREEN_HEIGHT - 50 - h;

        /* Gradient effect */
        for (int j = 0; j < h; j += 5) {
            float ratio = (float)j / h;
            uint8_t green = 150 + (int)(ratio * 100);
            draw_filled_rect(layer, x, y + j, bar_w, 5,
                           lv_color_make(0, green, 0), LV_OPA_COVER);
        }
    }
}

static void draw_wifi_signal(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x1565C0);

    int phase = ((int)(t * 2)) % 4;

    /* Draw arcs based on phase */
    for (int i = 0; i < 3; i++) {
        if (i < phase) {
            int r = 30 + i * 25;
            draw_arc(layer, CENTER_X, CENTER_Y + 50, r, 225, 315, 6,
                    lv_color_hex(0xFFFFFF), LV_OPA_COVER);
        } else {
            int r = 30 + i * 25;
            draw_arc(layer, CENTER_X, CENTER_Y + 50, r, 225, 315, 6,
                    lv_color_hex(0x42A5F5), LV_OPA_50);
        }
    }

    /* Center dot */
    draw_circle(layer, CENTER_X, CENTER_Y + 50, 8, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
}

static void draw_battery_charging(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x263238);

    float level = fmodf(t * 0.3f, 1.0f);

    /* Battery outline */
    int bx = CENTER_X - 50, by = CENTER_Y - 30;
    int bw = 100, bh = 60;

    draw_filled_rect(layer, bx, by, bw, bh, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
    draw_filled_rect(layer, bx + 3, by + 3, bw - 6, bh - 6,
                    lv_color_hex(0x263238), LV_OPA_COVER);

    /* Terminal */
    draw_filled_rect(layer, bx + bw, by + 15, 8, 30, lv_color_hex(0xFFFFFF), LV_OPA_COVER);

    /* Fill level */
    int fill_w = (int)((bw - 10) * level);
    lv_color_t fill_color = (level < 0.2f) ? lv_color_hex(0xF44336) :
                            (level < 0.5f) ? lv_color_hex(0xFF9800) :
                                             lv_color_hex(0x4CAF50);
    draw_filled_rect(layer, bx + 5, by + 5, fill_w, bh - 10, fill_color, LV_OPA_COVER);

    /* Lightning bolt overlay */
    if (fmodf(t, 1.0f) < 0.7f) {
        int lx = CENTER_X;
        draw_line(layer, lx + 10, by + 10, lx - 5, by + bh/2, lv_color_hex(0xFFEB3B), 4);
        draw_line(layer, lx - 5, by + bh/2, lx + 5, by + bh/2, lv_color_hex(0xFFEB3B), 4);
        draw_line(layer, lx + 5, by + bh/2, lx - 10, by + bh - 10, lv_color_hex(0xFFEB3B), 4);
    }
}

static void draw_binary_code(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x001100);

    s_state.scroll_offset = fmodf(s_state.scroll_offset + 1, 20);

    /* Update random digits occasionally */
    if (rand() % 3 == 0) {
        int col = rand() % 6;
        int row = rand() % 10;
        s_state.binary_cols[col][row] = '0' + (rand() % 2);
    }

    /* Draw columns */
    for (int col = 0; col < 6; col++) {
        int x = 30 + col * 35;
        for (int row = 0; row < 12; row++) {
            int y = (int)s_state.scroll_offset + row * 22 - 20;
            if (y < 0) y += SCREEN_HEIGHT + 40;
            if (y > SCREEN_HEIGHT) continue;

            char digit = s_state.binary_cols[col][row % 10];
            if (digit == 0) digit = '0' + (rand() % 2);

            lv_opa_t opa = 255 - (row * 15);
            draw_filled_rect(layer, x, y, 12, 16,
                           lv_color_make(0, 255 - row * 10, 0), opa);
        }
    }
}

/*===========================================================================
 * Nature/Organic Animations
 *===========================================================================*/

static void draw_bouncing_ball(lv_layer_t *layer, float t)
{
    fill_background(layer, 0xECEFF1);

    /* Physics simulation */
    s_state.ball_vy += 500 * (1.0f / ANIM_FPS);  /* Gravity */
    s_state.ball_y += s_state.ball_vy * (1.0f / ANIM_FPS);

    /* Bounce */
    if (s_state.ball_y > SCREEN_HEIGHT - 60) {
        s_state.ball_y = SCREEN_HEIGHT - 60;
        s_state.ball_vy = -s_state.ball_vy * 0.8f;
        if (fabsf(s_state.ball_vy) < 50) {
            s_state.ball_vy = -400;  /* Reset */
        }
    }

    /* Shadow */
    float shadow_scale = 1.0f - (SCREEN_HEIGHT - 60 - s_state.ball_y) / 200.0f;
    if (shadow_scale > 0) {
        int shadow_r = (int)(25 * shadow_scale);
        draw_circle(layer, CENTER_X, SCREEN_HEIGHT - 30, shadow_r,
                   lv_color_hex(0x424242), LV_OPA_30);
    }

    /* Ball with squash */
    float squash = fabsf(s_state.ball_vy) / 600.0f;
    if (s_state.ball_y > SCREEN_HEIGHT - 80) {
        squash = 0.3f;
    }

    int ball_h = (int)(30 * (1 - squash * 0.3f));
    int ball_w = (int)(30 * (1 + squash * 0.2f));

    /* Ball body */
    draw_circle(layer, CENTER_X, (int)s_state.ball_y, ball_w,
               lv_color_hex(0xF44336), LV_OPA_COVER);
    /* Highlight */
    draw_circle(layer, CENTER_X - 8, (int)s_state.ball_y - 10, 6,
               lv_color_hex(0xFFCDD2), LV_OPA_COVER);
}

static void draw_ocean_waves(lv_layer_t *layer, float t)
{
    /* Sky gradient (simplified) */
    draw_filled_rect(layer, 0, 0, SCREEN_WIDTH, CENTER_Y,
                    lv_color_hex(0x87CEEB), LV_OPA_COVER);
    /* Ocean base */
    draw_filled_rect(layer, 0, CENTER_Y, SCREEN_WIDTH, CENTER_Y + 42,
                    lv_color_hex(0x1565C0), LV_OPA_COVER);

    /* Draw wave layers */
    uint32_t wave_colors[] = {0x42A5F5, 0x1E88E5, 0x1565C0};

    for (int w = 0; w < 3; w++) {
        int base_y = CENTER_Y + 20 + w * 40;

        for (int x = 0; x < SCREEN_WIDTH; x += 2) {
            float wave = sinf(x * 0.04f + t * 2 - w * 0.5f) * (15 - w * 3);
            int y = base_y + (int)wave;

            draw_filled_rect(layer, x, y, 3, SCREEN_HEIGHT - y,
                           lv_color_hex(wave_colors[w]), LV_OPA_COVER);
        }
    }
}

static void draw_butterfly(lv_layer_t *layer, float t)
{
    fill_background(layer, 0xE8F5E9);

    /* Flight path */
    float path_x = CENTER_X + sinf(t * 0.7f) * 60;
    float path_y = CENTER_Y + sinf(t * 1.1f) * 40;

    /* Wing flap */
    float flap = fabsf(sinf(t * 10));
    int wing_w = (int)(25 * flap);

    /* Left wing */
    draw_circle(layer, (int)path_x - wing_w, (int)path_y, 20,
               lv_color_hex(0xFF9800), LV_OPA_COVER);
    draw_circle(layer, (int)path_x - wing_w - 10, (int)path_y + 15, 12,
               lv_color_hex(0xFFB74D), LV_OPA_COVER);

    /* Right wing */
    draw_circle(layer, (int)path_x + wing_w, (int)path_y, 20,
               lv_color_hex(0xFF9800), LV_OPA_COVER);
    draw_circle(layer, (int)path_x + wing_w + 10, (int)path_y + 15, 12,
               lv_color_hex(0xFFB74D), LV_OPA_COVER);

    /* Body */
    draw_filled_rect(layer, (int)path_x - 3, (int)path_y - 15, 6, 35,
                    lv_color_hex(0x3E2723), LV_OPA_COVER);

    /* Antennae */
    draw_line(layer, (int)path_x - 2, (int)path_y - 15,
              (int)path_x - 10, (int)path_y - 25, lv_color_hex(0x3E2723), 1);
    draw_line(layer, (int)path_x + 2, (int)path_y - 15,
              (int)path_x + 10, (int)path_y - 25, lv_color_hex(0x3E2723), 1);
}

static void draw_fireworks(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x0D1B2A);

    /* Launch cycle */
    float cycle = fmodf(t, 3.0f);

    if (cycle < 1.0f && !s_state.launching) {
        /* Start new launch */
        s_state.launching = true;
        s_state.launch_x = 40 + (rand() % 160);
        s_state.launch_y = SCREEN_HEIGHT;
        s_state.launch_vy = -300;
        s_state.spark_count = 0;
    }

    if (s_state.launching) {
        /* Update launch position */
        s_state.launch_vy += 200 * (1.0f / ANIM_FPS);
        s_state.launch_y += s_state.launch_vy * (1.0f / ANIM_FPS);

        /* Draw trail */
        draw_circle(layer, (int)s_state.launch_x, (int)s_state.launch_y, 3,
                   lv_color_hex(0xFFFFFF), LV_OPA_COVER);

        /* Explode when velocity reverses */
        if (s_state.launch_vy > 0) {
            s_state.launching = false;
            s_state.spark_count = 30;

            /* Create sparks */
            for (int i = 0; i < 30; i++) {
                float angle = (float)(rand() % 360) * M_PI / 180;
                float speed = 50 + (rand() % 150);
                s_state.sparks[i].x = s_state.launch_x;
                s_state.sparks[i].y = s_state.launch_y;
                s_state.sparks[i].vx = cosf(angle) * speed;
                s_state.sparks[i].vy = sinf(angle) * speed;
                s_state.sparks[i].life = 1.0f;
            }
        }
    }

    /* Update and draw sparks */
    for (int i = 0; i < s_state.spark_count; i++) {
        if (s_state.sparks[i].life > 0) {
            s_state.sparks[i].x += s_state.sparks[i].vx * (1.0f / ANIM_FPS);
            s_state.sparks[i].y += s_state.sparks[i].vy * (1.0f / ANIM_FPS);
            s_state.sparks[i].vy += 100 * (1.0f / ANIM_FPS);  /* Gravity */
            s_state.sparks[i].life -= 0.02f;

            lv_opa_t opa = (lv_opa_t)(s_state.sparks[i].life * 255);
            uint32_t colors[] = {0xFF5722, 0xFFEB3B, 0xE91E63, 0x00BCD4};
            draw_circle(layer, (int)s_state.sparks[i].x, (int)s_state.sparks[i].y,
                       3, lv_color_hex(colors[i % 4]), opa);
        }
    }
}

static void draw_campfire(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x1A1A2E);

    /* Ground */
    draw_filled_rect(layer, 0, SCREEN_HEIGHT - 40, SCREEN_WIDTH, 40,
                    lv_color_hex(0x3E2723), LV_OPA_COVER);

    /* Logs */
    draw_filled_rect(layer, CENTER_X - 50, SCREEN_HEIGHT - 55, 40, 15,
                    lv_color_hex(0x5D4037), LV_OPA_COVER);
    draw_filled_rect(layer, CENTER_X + 10, SCREEN_HEIGHT - 55, 40, 15,
                    lv_color_hex(0x5D4037), LV_OPA_COVER);
    draw_filled_rect(layer, CENTER_X - 30, SCREEN_HEIGHT - 60, 60, 12,
                    lv_color_hex(0x4E342E), LV_OPA_COVER);

    /* Flames */
    uint32_t flame_colors[] = {0xFFEB3B, 0xFF9800, 0xFF5722, 0xF44336};

    for (int i = 0; i < 5; i++) {
        float flicker = sinf(t * 10 + i * 2) * 10 + (rand() % 5);
        int flame_h = 50 + (int)flicker + (rand() % 20);
        int flame_x = CENTER_X - 30 + i * 15;
        int flame_y = SCREEN_HEIGHT - 60 - flame_h;

        /* Draw flame as stacked circles */
        for (int j = 0; j < flame_h; j += 8) {
            int r = 12 - (j * 10 / flame_h);
            if (r < 3) r = 3;
            int ci = j * 4 / flame_h;
            draw_circle(layer, flame_x, flame_y + j, r,
                       lv_color_hex(flame_colors[ci]), LV_OPA_80);
        }
    }

    /* Embers */
    for (int i = 0; i < 8; i++) {
        float ember_y = CENTER_Y - fmodf(t * 50 + i * 30, 150);
        float ember_x = CENTER_X + sinf(t * 3 + i) * 30;
        lv_opa_t opa = (lv_opa_t)((CENTER_Y - ember_y) * 2);
        draw_circle(layer, (int)ember_x, (int)ember_y, 2, lv_color_hex(0xFFAB00), opa);
    }
}

static void draw_bubbles(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x0288D1);

    /* Initialize bubbles */
    if (s_state.particle_count == 0) {
        s_state.particle_count = 15;
        for (int i = 0; i < 15; i++) {
            s_state.particles[i].x = rand() % SCREEN_WIDTH;
            s_state.particles[i].y = SCREEN_HEIGHT + (rand() % 100);
            s_state.particles[i].speed = 40 + (rand() % 60);
        }
    }

    for (int i = 0; i < 15; i++) {
        /* Wobble */
        s_state.particles[i].x += sinf(t * 2 + i) * 0.3f;
        s_state.particles[i].y -= s_state.particles[i].speed * (1.0f / ANIM_FPS);

        /* Pop and reset at top */
        if (s_state.particles[i].y < 20) {
            s_state.particles[i].y = SCREEN_HEIGHT + 20;
            s_state.particles[i].x = rand() % SCREEN_WIDTH;
        }

        int r = 8 + (i % 5) * 4;
        int x = (int)s_state.particles[i].x;
        int y = (int)s_state.particles[i].y;

        /* Bubble body */
        draw_circle(layer, x, y, r, lv_color_hex(0x81D4FA), LV_OPA_60);
        /* Highlight */
        draw_circle(layer, x - r/3, y - r/3, r/3, lv_color_hex(0xFFFFFF), LV_OPA_80);
    }
}

/*===========================================================================
 * Dashboard/Automotive Animations
 *===========================================================================*/

static void draw_speedometer(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x212121);

    int gauge_cx = CENTER_X;
    int gauge_cy = CENTER_Y + 30;
    int gauge_r = 90;

    /* Gauge background */
    draw_arc(layer, gauge_cx, gauge_cy, gauge_r, 135, 405, 20,
             lv_color_hex(0x424242), LV_OPA_COVER);

    /* Red zone */
    draw_arc(layer, gauge_cx, gauge_cy, gauge_r, 350, 405, 20,
             lv_color_hex(0xF44336), LV_OPA_COVER);

    /* Tick marks and labels */
    for (int i = 0; i <= 8; i++) {
        float angle = (135 + i * 33.75f) * M_PI / 180;
        int x1 = gauge_cx + (int)(cosf(angle) * (gauge_r - 25));
        int y1 = gauge_cy + (int)(sinf(angle) * (gauge_r - 25));
        int x2 = gauge_cx + (int)(cosf(angle) * (gauge_r - 5));
        int y2 = gauge_cy + (int)(sinf(angle) * (gauge_r - 5));
        draw_line(layer, x1, y1, x2, y2, lv_color_hex(0xFFFFFF), 2);
    }

    /* Needle */
    float speed = (1.0f + sinf(t * 0.5f)) * 0.5f;  /* 0-1 */
    float needle_angle = (135 + speed * 270) * M_PI / 180;
    int nx = gauge_cx + (int)(cosf(needle_angle) * (gauge_r - 30));
    int ny = gauge_cy + (int)(sinf(needle_angle) * (gauge_r - 30));

    draw_line(layer, gauge_cx, gauge_cy, nx, ny, lv_color_hex(0xF44336), 4);
    draw_circle(layer, gauge_cx, gauge_cy, 10, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
}

static void draw_fuel_gauge(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x212121);

    int gauge_cx = CENTER_X;
    int gauge_cy = CENTER_Y + 40;
    int gauge_r = 70;

    /* Gauge arc */
    draw_arc(layer, gauge_cx, gauge_cy, gauge_r, 180, 360, 15,
             lv_color_hex(0x424242), LV_OPA_COVER);

    /* E and F labels (using circles as markers) */
    draw_circle(layer, gauge_cx - 70, gauge_cy, 6, lv_color_hex(0xF44336), LV_OPA_COVER);
    draw_circle(layer, gauge_cx + 70, gauge_cy, 6, lv_color_hex(0x4CAF50), LV_OPA_COVER);

    /* Fuel level - oscillate */
    float level = 0.3f + 0.5f * (1.0f + sinf(t * 0.3f)) / 2.0f;
    float needle_angle = (180 + level * 180) * M_PI / 180;

    int nx = gauge_cx + (int)(cosf(needle_angle) * (gauge_r - 15));
    int ny = gauge_cy + (int)(sinf(needle_angle) * (gauge_r - 15));

    lv_color_t needle_color = (level < 0.2f) ? lv_color_hex(0xF44336) : lv_color_hex(0xFF9800);
    draw_line(layer, gauge_cx, gauge_cy, nx, ny, needle_color, 4);
    draw_circle(layer, gauge_cx, gauge_cy, 8, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
}

static void draw_turn_signal(lv_layer_t *layer, float t, bool left)
{
    fill_background(layer, 0x212121);

    bool on = fmodf(t, 1.0f) < 0.5f;

    int arrow_x = CENTER_X + (left ? -30 : 30);
    int dir = left ? -1 : 1;

    lv_color_t color = on ? lv_color_hex(0x4CAF50) : lv_color_hex(0x1B5E20);
    lv_opa_t opa = on ? LV_OPA_COVER : LV_OPA_30;

    /* Arrow shape */
    draw_line(layer, arrow_x, CENTER_Y, arrow_x + dir * 40, CENTER_Y - 40, color, 8);
    draw_line(layer, arrow_x, CENTER_Y, arrow_x + dir * 40, CENTER_Y + 40, color, 8);
    draw_line(layer, arrow_x, CENTER_Y - 25, arrow_x, CENTER_Y + 25, color, 8);
}

static void draw_turn_left(lv_layer_t *layer, float t)
{
    draw_turn_signal(layer, t, true);
}

static void draw_turn_right(lv_layer_t *layer, float t)
{
    draw_turn_signal(layer, t, false);
}

static void draw_hazard_lights(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x212121);

    bool on = fmodf(t, 0.6f) < 0.3f;
    lv_color_t color = on ? lv_color_hex(0xFF9800) : lv_color_hex(0x5D4037);
    lv_opa_t opa = on ? LV_OPA_COVER : LV_OPA_30;

    /* Left arrow */
    int ax = CENTER_X - 60;
    draw_line(layer, ax, CENTER_Y, ax - 30, CENTER_Y - 30, color, 6);
    draw_line(layer, ax, CENTER_Y, ax - 30, CENTER_Y + 30, color, 6);
    draw_line(layer, ax, CENTER_Y - 20, ax, CENTER_Y + 20, color, 6);

    /* Right arrow */
    ax = CENTER_X + 60;
    draw_line(layer, ax, CENTER_Y, ax + 30, CENTER_Y - 30, color, 6);
    draw_line(layer, ax, CENTER_Y, ax + 30, CENTER_Y + 30, color, 6);
    draw_line(layer, ax, CENTER_Y - 20, ax, CENTER_Y + 20, color, 6);
}

static void draw_gear_display(lv_layer_t *layer, float t)
{
    fill_background(layer, 0x0D1B2A);

    /* Update gear periodically */
    s_state.gear_timer += 1.0f / ANIM_FPS;
    if (s_state.gear_timer > 2.0f) {
        s_state.gear_timer = 0;
        s_state.current_gear = (s_state.current_gear % 6) + 1;
    }

    /* Draw current gear number large */
    int gear = s_state.current_gear;

    /* Gear indicator circles */
    for (int i = 1; i <= 6; i++) {
        int dot_x = 40 + (i - 1) * 32;
        int dot_y = SCREEN_HEIGHT - 50;
        lv_color_t color = (i <= gear) ? lv_color_hex(0x2196F3) : lv_color_hex(0x37474F);
        draw_circle(layer, dot_x, dot_y, 10, color, LV_OPA_COVER);
    }

    /* Large gear number - draw using lines to form digit */
    int gx = CENTER_X, gy = CENTER_Y - 20;
    int seg_w = 40, seg_h = 60;

    /* Define which segments are on for each digit 1-6 */
    /* Simplified 7-segment display */
    lv_color_t on_color = lv_color_hex(0xFFFFFF);

    /* Draw rectangular approximation of number */
    draw_filled_rect(layer, gx - 20, gy - 40, 40, 80, lv_color_hex(0x1565C0), LV_OPA_COVER);

    /* Overlay with gear number pattern (simplified) */
    switch (gear) {
        case 1:
            draw_filled_rect(layer, gx - 5, gy - 35, 10, 70, on_color, LV_OPA_COVER);
            break;
        case 2:
            draw_filled_rect(layer, gx - 15, gy - 35, 30, 10, on_color, LV_OPA_COVER);
            draw_filled_rect(layer, gx + 5, gy - 35, 10, 35, on_color, LV_OPA_COVER);
            draw_filled_rect(layer, gx - 15, gy, 30, 10, on_color, LV_OPA_COVER);
            draw_filled_rect(layer, gx - 15, gy, 10, 35, on_color, LV_OPA_COVER);
            draw_filled_rect(layer, gx - 15, gy + 25, 30, 10, on_color, LV_OPA_COVER);
            break;
        case 3:
            draw_filled_rect(layer, gx - 15, gy - 35, 30, 10, on_color, LV_OPA_COVER);
            draw_filled_rect(layer, gx + 5, gy - 35, 10, 70, on_color, LV_OPA_COVER);
            draw_filled_rect(layer, gx - 15, gy, 30, 10, on_color, LV_OPA_COVER);
            draw_filled_rect(layer, gx - 15, gy + 25, 30, 10, on_color, LV_OPA_COVER);
            break;
        case 4:
            draw_filled_rect(layer, gx - 15, gy - 35, 10, 40, on_color, LV_OPA_COVER);
            draw_filled_rect(layer, gx + 5, gy - 35, 10, 70, on_color, LV_OPA_COVER);
            draw_filled_rect(layer, gx - 15, gy, 30, 10, on_color, LV_OPA_COVER);
            break;
        case 5:
            draw_filled_rect(layer, gx - 15, gy - 35, 30, 10, on_color, LV_OPA_COVER);
            draw_filled_rect(layer, gx - 15, gy - 35, 10, 40, on_color, LV_OPA_COVER);
            draw_filled_rect(layer, gx - 15, gy, 30, 10, on_color, LV_OPA_COVER);
            draw_filled_rect(layer, gx + 5, gy, 10, 35, on_color, LV_OPA_COVER);
            draw_filled_rect(layer, gx - 15, gy + 25, 30, 10, on_color, LV_OPA_COVER);
            break;
        case 6:
            draw_filled_rect(layer, gx - 15, gy - 35, 30, 10, on_color, LV_OPA_COVER);
            draw_filled_rect(layer, gx - 15, gy - 35, 10, 70, on_color, LV_OPA_COVER);
            draw_filled_rect(layer, gx - 15, gy, 30, 10, on_color, LV_OPA_COVER);
            draw_filled_rect(layer, gx + 5, gy, 10, 35, on_color, LV_OPA_COVER);
            draw_filled_rect(layer, gx - 15, gy + 25, 30, 10, on_color, LV_OPA_COVER);
            break;
    }
}

/*===========================================================================
 * Animation Draw Function Table
 *===========================================================================*/

typedef void (*draw_func_t)(lv_layer_t *layer, float t);

static const draw_func_t s_draw_funcs[GALLERY_ANIM_MAX] = {
    /* Abstract Geometric */
    draw_pulsing_rings,
    draw_spiral_galaxy,
    draw_heartbeat,
    draw_breathing_orb,
    draw_matrix_rain,
    draw_radar_sweep,

    /* Weather Effects */
    draw_rain_storm,
    draw_snowfall,
    draw_sunshine,
    draw_lightning,
    draw_starry_night,
    draw_aurora,

    /* Emoji/Symbols */
    draw_floating_hearts,
    draw_star_burst,
    draw_question_mark,
    draw_exclamation,
    draw_checkmark,
    draw_x_mark,

    /* Tech/Digital */
    draw_loading_spinner,
    draw_progress_bar,
    draw_sound_waves,
    draw_wifi_signal,
    draw_battery_charging,
    draw_binary_code,

    /* Nature/Organic */
    draw_bouncing_ball,
    draw_ocean_waves,
    draw_butterfly,
    draw_fireworks,
    draw_campfire,
    draw_bubbles,

    /* Dashboard/Automotive */
    draw_speedometer,
    draw_fuel_gauge,
    draw_turn_left,
    draw_turn_right,
    draw_hazard_lights,
    draw_gear_display,
};

/*===========================================================================
 * Drawing Callback
 *===========================================================================*/

static void draw_cb(lv_event_t *e)
{
    if (!s_visible) return;

    lv_layer_t *layer = lv_event_get_layer(e);
    if (layer == NULL) return;

    if (s_current_anim < GALLERY_ANIM_MAX && s_draw_funcs[s_current_anim] != NULL) {
        s_draw_funcs[s_current_anim](layer, s_time);
    }
}

/*===========================================================================
 * Animation Timer
 *===========================================================================*/

static void anim_timer_cb(lv_timer_t *timer)
{
    s_time += 1.0f / ANIM_FPS;

    if (s_draw_obj && s_visible) {
        lv_obj_invalidate(s_draw_obj);
    }
}

/*===========================================================================
 * Public API
 *===========================================================================*/

int gallery_anim_init(lv_obj_t *parent)
{
    if (s_draw_obj != NULL) {
        ESP_LOGW(TAG, "Already initialized");
        return 0;
    }

    ESP_LOGI(TAG, "Initializing gallery animations");

    /* Reset state */
    memset(&s_state, 0, sizeof(s_state));
    s_time = 0;
    s_visible = false;

    /* Create drawing object */
    s_draw_obj = lv_obj_create(parent);
    lv_obj_remove_style_all(s_draw_obj);
    lv_obj_set_size(s_draw_obj, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_center(s_draw_obj);
    lv_obj_add_flag(s_draw_obj, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_clear_flag(s_draw_obj, LV_OBJ_FLAG_SCROLLABLE);

    /* Register draw callback */
    lv_obj_add_event_cb(s_draw_obj, draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    /* Start animation timer */
    s_anim_timer = lv_timer_create(anim_timer_cb, ANIM_PERIOD_MS, NULL);

    /* Initially hidden */
    lv_obj_add_flag(s_draw_obj, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "Gallery animations initialized (36 animations)");
    return 0;
}

void gallery_anim_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing gallery animations");

    if (s_anim_timer) {
        lv_timer_delete(s_anim_timer);
        s_anim_timer = NULL;
    }

    if (s_draw_obj) {
        lv_obj_delete(s_draw_obj);
        s_draw_obj = NULL;
    }

    memset(&s_state, 0, sizeof(s_state));
    s_visible = false;
}

void gallery_anim_set(gallery_anim_id_t anim_id)
{
    if (anim_id >= GALLERY_ANIM_MAX) {
        ESP_LOGE(TAG, "Invalid animation ID: %d", anim_id);
        return;
    }

    s_current_anim = anim_id;
    s_time = 0;  /* Reset time for new animation */

    /* Reset animation-specific state */
    memset(&s_state, 0, sizeof(s_state));

    ESP_LOGI(TAG, "Set animation: %s", s_anim_info[anim_id].name);
}

gallery_anim_id_t gallery_anim_get(void)
{
    return s_current_anim;
}

const gallery_anim_info_t* gallery_anim_get_info(gallery_anim_id_t anim_id)
{
    if (anim_id >= GALLERY_ANIM_MAX) return NULL;
    return &s_anim_info[anim_id];
}

int gallery_anim_get_count(void)
{
    return GALLERY_ANIM_MAX;
}

void gallery_anim_set_visible(bool visible)
{
    s_visible = visible;

    if (s_draw_obj) {
        if (visible) {
            lv_obj_remove_flag(s_draw_obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_draw_obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

bool gallery_anim_is_visible(void)
{
    return s_visible;
}
