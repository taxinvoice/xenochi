/**
 * @file mochi_particles.c
 * @brief MochiState Particle Effects - Animated background particles
 *
 * Particle types:
 * - Float: Gentle floating circles
 * - Burst: Expanding ring of circles
 * - Sweat: Falling sweat drops
 * - Sparkle: Rotating star shapes
 * - Spiral: Rotating @ symbols (for dizzy)
 * - ZZZ: Floating Z letters (for sleepy)
 */

#include "mochi_state.h"
#include "mochi_theme.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "mochi_particles";

/*===========================================================================
 * Constants
 *===========================================================================*/

#define PI                      3.14159265358979f
#define PARTICLE_TIMER_MS       33  /* ~30 FPS */
#define DISPLAY_WIDTH           240
#define DISPLAY_HEIGHT          284
#define CENTER_X                120
#define CENTER_Y                142

/* Particle counts */
#define FLOAT_COUNT             5
#define BURST_COUNT             8
#define SWEAT_COUNT             2
#define SPARKLE_COUNT           4
#define SPIRAL_COUNT            3

/*===========================================================================
 * Static Variables
 *===========================================================================*/

static struct {
    lv_obj_t *container;
    lv_obj_t *particles[8];  /* Up to 8 particle objects */
    lv_obj_t *labels[3];     /* For ZZZ text */
    lv_timer_t *timer;

    mochi_particle_type_t current_type;
    const mochi_theme_t *theme;
    uint32_t frame;
    int particle_count;
} s_particles = {
    .container = NULL,
    .timer = NULL,
    .current_type = MOCHI_PARTICLE_NONE,
    .theme = NULL,
    .frame = 0,
    .particle_count = 0,
};

/*===========================================================================
 * Particle Creation Functions
 *===========================================================================*/

static void clear_particles(void) {
    /* Delete all particle objects */
    for (int i = 0; i < 8; i++) {
        if (s_particles.particles[i] != NULL) {
            lv_obj_delete(s_particles.particles[i]);
            s_particles.particles[i] = NULL;
        }
    }
    for (int i = 0; i < 3; i++) {
        if (s_particles.labels[i] != NULL) {
            lv_obj_delete(s_particles.labels[i]);
            s_particles.labels[i] = NULL;
        }
    }
    s_particles.particle_count = 0;
}

static void create_float_particles(void) {
    for (int i = 0; i < FLOAT_COUNT && i < 8; i++) {
        lv_obj_t *p = lv_obj_create(s_particles.container);
        lv_obj_remove_style_all(p);
        lv_obj_set_size(p, 8, 8);
        lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(p, s_particles.theme->particle, 0);
        lv_obj_set_style_bg_opa(p, LV_OPA_60, 0);
        s_particles.particles[i] = p;
    }
    s_particles.particle_count = FLOAT_COUNT;
}

static void create_burst_particles(void) {
    for (int i = 0; i < BURST_COUNT && i < 8; i++) {
        lv_obj_t *p = lv_obj_create(s_particles.container);
        lv_obj_remove_style_all(p);
        lv_obj_set_size(p, 8, 8);
        lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(p, s_particles.theme->accent, 0);
        lv_obj_set_style_bg_opa(p, LV_OPA_60, 0);
        s_particles.particles[i] = p;
    }
    s_particles.particle_count = BURST_COUNT;
}

static void create_sweat_particles(void) {
    for (int i = 0; i < SWEAT_COUNT && i < 8; i++) {
        lv_obj_t *p = lv_obj_create(s_particles.container);
        lv_obj_remove_style_all(p);
        lv_obj_set_size(p, 8, 12);
        lv_obj_set_style_radius(p, 4, 0);
        lv_obj_set_style_bg_color(p, lv_color_make(135, 206, 250), 0);  /* Light blue */
        lv_obj_set_style_bg_opa(p, LV_OPA_70, 0);
        s_particles.particles[i] = p;
    }
    s_particles.particle_count = SWEAT_COUNT;
}

static void create_sparkle_particles(void) {
    /* Sparkles are just circles that rotate around */
    for (int i = 0; i < SPARKLE_COUNT && i < 8; i++) {
        lv_obj_t *p = lv_obj_create(s_particles.container);
        lv_obj_remove_style_all(p);
        lv_obj_set_size(p, 10, 10);
        lv_obj_set_style_radius(p, 2, 0);  /* Star-like shape approximation */
        lv_obj_set_style_bg_color(p, s_particles.theme->accent, 0);
        lv_obj_set_style_bg_opa(p, LV_OPA_70, 0);
        s_particles.particles[i] = p;
    }
    s_particles.particle_count = SPARKLE_COUNT;
}

static void create_spiral_particles(void) {
    for (int i = 0; i < SPIRAL_COUNT && i < 8; i++) {
        lv_obj_t *p = lv_obj_create(s_particles.container);
        lv_obj_remove_style_all(p);
        lv_obj_set_size(p, 12, 12);
        lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_color(p, s_particles.theme->accent, 0);
        lv_obj_set_style_border_width(p, 2, 0);
        lv_obj_set_style_border_opa(p, LV_OPA_60, 0);
        lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, 0);
        s_particles.particles[i] = p;
    }
    s_particles.particle_count = SPIRAL_COUNT;
}

static void create_zzz_particles(void) {
    static lv_style_t style;
    static bool style_init = false;
    if (!style_init) {
        lv_style_init(&style);
        lv_style_set_text_font(&style, &lv_font_montserrat_14);
        style_init = true;
    }

    const char *texts[] = {"Z", "z", "z"};

    for (int i = 0; i < 3; i++) {
        lv_obj_t *label = lv_label_create(s_particles.container);
        lv_label_set_text(label, texts[i]);
        lv_obj_set_style_text_color(label, s_particles.theme->accent, 0);
        lv_obj_set_style_text_opa(label, LV_OPA_80 - i * 20, 0);
        s_particles.labels[i] = label;
    }
}

/*===========================================================================
 * Particle Update Functions
 *===========================================================================*/

static void update_float_particles(uint32_t frame) {
    float t = frame * PARTICLE_TIMER_MS / 1000.0f;

    for (int i = 0; i < FLOAT_COUNT && s_particles.particles[i] != NULL; i++) {
        int x = (int)(CENTER_X + sinf(t * 0.02f + i * 1.5f) * 100 - 50);
        int y = (int)(30 + cosf(t * 0.015f + i * 1.2f) * 60 + i * 25);
        int size = (int)(6 + sinf(t * 0.05f + i) * 2);

        lv_obj_set_pos(s_particles.particles[i], x - size/2, y - size/2);
        lv_obj_set_size(s_particles.particles[i], size, size);

        lv_opa_t opa = (lv_opa_t)(100 + sinf(t * 0.03f + i) * 50);
        lv_obj_set_style_bg_opa(s_particles.particles[i], opa, 0);
    }
}

static void update_burst_particles(uint32_t frame) {
    float t = frame * PARTICLE_TIMER_MS / 1000.0f;

    for (int i = 0; i < BURST_COUNT && s_particles.particles[i] != NULL; i++) {
        float angle = ((float)i / BURST_COUNT) * 2 * PI + t * 0.1f;
        float dist = 80 + sinf(t * 0.2f + i) * 20;
        int x = (int)(CENTER_X + cosf(angle) * dist);
        int y = (int)(CENTER_Y + sinf(angle) * dist * 0.6f);

        lv_obj_set_pos(s_particles.particles[i], x - 4, y - 4);
    }
}

static void update_sweat_particles(uint32_t frame) {
    int sweat_y = (frame * 2) % 60;

    if (s_particles.particles[0] != NULL) {
        lv_obj_set_pos(s_particles.particles[0], CENTER_X - 75, 60 + sweat_y);
    }
    if (s_particles.particles[1] != NULL) {
        lv_obj_set_pos(s_particles.particles[1], CENTER_X + 70, 50 + (sweat_y + 20) % 60);
    }
}

static void update_sparkle_particles(uint32_t frame) {
    float t = frame * PARTICLE_TIMER_MS / 1000.0f;

    for (int i = 0; i < SPARKLE_COUNT && s_particles.particles[i] != NULL; i++) {
        int x = (int)(80 + i * 45 + sinf(t * 0.1f + i) * 10);
        int y = (int)(50 + cosf(t * 0.08f + i * 2) * 30);

        lv_obj_set_pos(s_particles.particles[i], x - 5, y - 5);

        /* Rotate the sparkle (using transform if available) */
        int32_t angle = (int32_t)((frame * 3 + i * 45) % 360) * 10;
        lv_obj_set_style_transform_rotation(s_particles.particles[i], angle, 0);
    }
}

static void update_spiral_particles(uint32_t frame) {
    float t = frame * PARTICLE_TIMER_MS / 1000.0f;

    for (int i = 0; i < SPIRAL_COUNT && s_particles.particles[i] != NULL; i++) {
        float angle = t * 0.15f + i * 2;
        int x = (int)(CENTER_X + cosf(angle) * (40 + i * 20));
        int y = (int)(CENTER_Y - 30 + sinf(angle) * (30 + i * 15));

        lv_obj_set_pos(s_particles.particles[i], x - 6, y - 6);
    }
}

static void update_zzz_particles(uint32_t frame) {
    float t = frame * PARTICLE_TIMER_MS / 1000.0f;
    float offset = sinf(t * 0.05f) * 5;

    int base_x = CENTER_X + 55;
    int positions[][2] = {
        {(int)(base_x + offset), 60},
        {(int)(base_x + 15 + offset * 0.7f), 45},
        {(int)(base_x + 25 + offset * 0.5f), 35},
    };

    for (int i = 0; i < 3 && s_particles.labels[i] != NULL; i++) {
        lv_obj_set_pos(s_particles.labels[i], positions[i][0], positions[i][1]);
    }
}

/*===========================================================================
 * Timer Callback
 *===========================================================================*/

static void particles_timer_cb(lv_timer_t *timer) {
    s_particles.frame++;

    switch (s_particles.current_type) {
        case MOCHI_PARTICLE_FLOAT:
            update_float_particles(s_particles.frame);
            break;
        case MOCHI_PARTICLE_BURST:
            update_burst_particles(s_particles.frame);
            break;
        case MOCHI_PARTICLE_SWEAT:
            update_sweat_particles(s_particles.frame);
            break;
        case MOCHI_PARTICLE_SPARKLE:
            update_sparkle_particles(s_particles.frame);
            break;
        case MOCHI_PARTICLE_SPIRAL:
            update_spiral_particles(s_particles.frame);
            break;
        case MOCHI_PARTICLE_ZZZ:
            update_zzz_particles(s_particles.frame);
            break;
        default:
            break;
    }
}

/*===========================================================================
 * Public Functions
 *===========================================================================*/

void mochi_particles_create(lv_obj_t *parent) {
    if (s_particles.container != NULL) {
        ESP_LOGW(TAG, "Particles already created");
        return;
    }

    ESP_LOGI(TAG, "Creating particles container");

    /* Create transparent container for particles */
    s_particles.container = lv_obj_create(parent);
    lv_obj_remove_style_all(s_particles.container);
    lv_obj_set_size(s_particles.container, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_pos(s_particles.container, 0, 0);
    lv_obj_clear_flag(s_particles.container, LV_OBJ_FLAG_SCROLLABLE);

    /* Initialize arrays */
    for (int i = 0; i < 8; i++) {
        s_particles.particles[i] = NULL;
    }
    for (int i = 0; i < 3; i++) {
        s_particles.labels[i] = NULL;
    }

    s_particles.frame = 0;
    s_particles.current_type = MOCHI_PARTICLE_NONE;

    /* Create timer for particle animation */
    s_particles.timer = lv_timer_create(particles_timer_cb, PARTICLE_TIMER_MS, NULL);
    lv_timer_pause(s_particles.timer);
}

void mochi_particles_destroy(void) {
    if (s_particles.timer != NULL) {
        lv_timer_delete(s_particles.timer);
        s_particles.timer = NULL;
    }

    clear_particles();

    if (s_particles.container != NULL) {
        lv_obj_delete(s_particles.container);
        s_particles.container = NULL;
    }
}

void mochi_particles_set_type(mochi_particle_type_t type, const mochi_theme_t *theme) {
    if (s_particles.container == NULL) return;

    /* If same type and theme, do nothing */
    if (type == s_particles.current_type && theme == s_particles.theme) {
        return;
    }

    ESP_LOGI(TAG, "Setting particle type: %d", type);

    s_particles.current_type = type;
    s_particles.theme = theme;
    s_particles.frame = 0;

    /* Clear existing particles */
    clear_particles();

    /* Create new particles based on type */
    switch (type) {
        case MOCHI_PARTICLE_FLOAT:
            create_float_particles();
            lv_timer_resume(s_particles.timer);
            break;
        case MOCHI_PARTICLE_BURST:
            create_burst_particles();
            lv_timer_resume(s_particles.timer);
            break;
        case MOCHI_PARTICLE_SWEAT:
            create_sweat_particles();
            lv_timer_resume(s_particles.timer);
            break;
        case MOCHI_PARTICLE_SPARKLE:
            create_sparkle_particles();
            lv_timer_resume(s_particles.timer);
            break;
        case MOCHI_PARTICLE_SPIRAL:
            create_spiral_particles();
            lv_timer_resume(s_particles.timer);
            break;
        case MOCHI_PARTICLE_ZZZ:
            create_zzz_particles();
            lv_timer_resume(s_particles.timer);
            break;
        case MOCHI_PARTICLE_NONE:
        default:
            lv_timer_pause(s_particles.timer);
            break;
    }
}

void mochi_particles_update(int frame) {
    /* Called externally if needed - currently handled by timer */
    s_particles.frame = frame;
}
