/**
 * @file car_gallery_data.c
 * @brief Car Animation Gallery - Animation catalog (68 animations)
 *
 * Defines:
 * - 32 car-themed face animations using mochi states
 * - 36 creative non-face animations using custom rendering
 */

#include "car_gallery_data.h"

/*===========================================================================
 * Category Names
 *===========================================================================*/

static const char *s_category_names[] = {
    "Engine",        /* CAR_CAT_ENGINE */
    "Driving",       /* CAR_CAT_DRIVING */
    "Speed",         /* CAR_CAT_SPEED */
    "Environment",   /* CAR_CAT_ENVIRONMENT */
    "Parking",       /* CAR_CAT_PARKING */
    "Safety",        /* CAR_CAT_SAFETY */
    "Entertainment", /* CAR_CAT_ENTERTAINMENT */
    "Geometric",     /* CAR_CAT_GEOMETRIC */
    "Weather",       /* CAR_CAT_WEATHER */
    "Symbols",       /* CAR_CAT_SYMBOLS */
    "Tech",          /* CAR_CAT_TECH */
    "Nature",        /* CAR_CAT_NATURE */
    "Dashboard",     /* CAR_CAT_DASHBOARD */
    "All",           /* CAR_CAT_ALL */
};

/*===========================================================================
 * Animation Catalog (68 Animations: 32 Face + 36 Custom)
 *===========================================================================*/

static const car_animation_t s_animations[] = {
    /*=========================================================================
     * FACE ANIMATIONS (32) - Using mochi state system
     *=======================================================================*/

    /*-------------------------------------------------------------------------
     * Category: Engine/Startup (4 states)
     *-----------------------------------------------------------------------*/
    {
        .name = "Ignition",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_ENGINE,
        .trigger_desc = "Engine starts / power on",
        .state = MOCHI_STATE_EXCITED,
        .activity = MOCHI_ACTIVITY_BOUNCE,
        .theme = MOCHI_THEME_PEACH,
        .custom_id = 0,
    },
    {
        .name = "Engine Idle",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_ENGINE,
        .trigger_desc = "Subtle vibration detected",
        .state = MOCHI_STATE_HAPPY,
        .activity = MOCHI_ACTIVITY_IDLE,
        .theme = MOCHI_THEME_MINT,
        .custom_id = 0,
    },
    {
        .name = "Warming Up",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_ENGINE,
        .trigger_desc = "Cold temperature",
        .state = MOCHI_STATE_SLEEPY,
        .activity = MOCHI_ACTIVITY_SHAKE,
        .theme = MOCHI_THEME_CLOUD,
        .custom_id = 0,
    },
    {
        .name = "Engine Off",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_ENGINE,
        .trigger_desc = "Complete stillness",
        .state = MOCHI_STATE_SLEEPY,
        .activity = MOCHI_ACTIVITY_SNORE,
        .theme = MOCHI_THEME_LAVENDER,
        .custom_id = 0,
    },

    /*-------------------------------------------------------------------------
     * Category: Driving Dynamics (8 states)
     *-----------------------------------------------------------------------*/
    {
        .name = "Cruising",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_DRIVING,
        .trigger_desc = "Steady forward motion",
        .state = MOCHI_STATE_COOL,
        .activity = MOCHI_ACTIVITY_IDLE,
        .theme = MOCHI_THEME_CLOUD,
        .custom_id = 0,
    },
    {
        .name = "Accelerating",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_DRIVING,
        .trigger_desc = "Strong acceleration",
        .state = MOCHI_STATE_EXCITED,
        .activity = MOCHI_ACTIVITY_SLIDE_DOWN,
        .theme = MOCHI_THEME_PEACH,
        .custom_id = 0,
    },
    {
        .name = "Hard Braking",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_DRIVING,
        .trigger_desc = "Rapid deceleration",
        .state = MOCHI_STATE_SHOCKED,
        .activity = MOCHI_ACTIVITY_SLIDE_UP,
        .theme = MOCHI_THEME_PEACH,
        .custom_id = 0,
    },
    {
        .name = "Left Turn",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_DRIVING,
        .trigger_desc = "Roll < 55 degrees",
        .state = MOCHI_STATE_HAPPY,
        .activity = MOCHI_ACTIVITY_SLIDE_LEFT,
        .theme = MOCHI_THEME_MINT,
        .custom_id = 0,
    },
    {
        .name = "Right Turn",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_DRIVING,
        .trigger_desc = "Roll > 125 degrees",
        .state = MOCHI_STATE_HAPPY,
        .activity = MOCHI_ACTIVITY_SLIDE_RIGHT,
        .theme = MOCHI_THEME_MINT,
        .custom_id = 0,
    },
    {
        .name = "Reversing",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_DRIVING,
        .trigger_desc = "Pitch < -20 degrees",
        .state = MOCHI_STATE_WORRIED,
        .activity = MOCHI_ACTIVITY_WIGGLE,
        .theme = MOCHI_THEME_LAVENDER,
        .custom_id = 0,
    },
    {
        .name = "Uphill",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_DRIVING,
        .trigger_desc = "Pitch > 15 degrees",
        .state = MOCHI_STATE_EXCITED,
        .activity = MOCHI_ACTIVITY_NOD,
        .theme = MOCHI_THEME_PEACH,
        .custom_id = 0,
    },
    {
        .name = "Downhill",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_DRIVING,
        .trigger_desc = "Pitch < -15 degrees",
        .state = MOCHI_STATE_EXCITED,
        .activity = MOCHI_ACTIVITY_SLIDE_DOWN,
        .theme = MOCHI_THEME_CLOUD,
        .custom_id = 0,
    },

    /*-------------------------------------------------------------------------
     * Category: Speed Zones (4 states)
     *-----------------------------------------------------------------------*/
    {
        .name = "Slow Zone",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_SPEED,
        .trigger_desc = "Very slow motion",
        .state = MOCHI_STATE_HAPPY,
        .activity = MOCHI_ACTIVITY_IDLE,
        .theme = MOCHI_THEME_SAKURA,
        .custom_id = 0,
    },
    {
        .name = "Highway Speed",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_SPEED,
        .trigger_desc = "Sustained high speed",
        .state = MOCHI_STATE_COOL,
        .activity = MOCHI_ACTIVITY_BLINK,
        .theme = MOCHI_THEME_CLOUD,
        .custom_id = 0,
    },
    {
        .name = "Speeding",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_SPEED,
        .trigger_desc = "Excessive speed warning",
        .state = MOCHI_STATE_PANIC,
        .activity = MOCHI_ACTIVITY_VIBRATE,
        .theme = MOCHI_THEME_PEACH,
        .custom_id = 0,
    },
    {
        .name = "Traffic Jam",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_SPEED,
        .trigger_desc = "Stop-and-go pattern",
        .state = MOCHI_STATE_WORRIED,
        .activity = MOCHI_ACTIVITY_IDLE,
        .theme = MOCHI_THEME_LAVENDER,
        .custom_id = 0,
    },

    /*-------------------------------------------------------------------------
     * Category: Environment/Weather (5 states)
     *-----------------------------------------------------------------------*/
    {
        .name = "Sunny Drive",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_ENVIRONMENT,
        .trigger_desc = "Daytime hours",
        .state = MOCHI_STATE_HAPPY,
        .activity = MOCHI_ACTIVITY_BOUNCE,
        .theme = MOCHI_THEME_PEACH,
        .custom_id = 0,
    },
    {
        .name = "Night Drive",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_ENVIRONMENT,
        .trigger_desc = "Nighttime driving",
        .state = MOCHI_STATE_COOL,
        .activity = MOCHI_ACTIVITY_BLINK,
        .theme = MOCHI_THEME_LAVENDER,
        .custom_id = 0,
    },
    {
        .name = "Rain Detected",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_ENVIRONMENT,
        .trigger_desc = "Vibration pattern",
        .state = MOCHI_STATE_WORRIED,
        .activity = MOCHI_ACTIVITY_SHAKE,
        .theme = MOCHI_THEME_CLOUD,
        .custom_id = 0,
    },
    {
        .name = "Bumpy Road",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_ENVIRONMENT,
        .trigger_desc = "Irregular vibrations",
        .state = MOCHI_STATE_DIZZY,
        .activity = MOCHI_ACTIVITY_SHAKE,
        .theme = MOCHI_THEME_PEACH,
        .custom_id = 0,
    },
    {
        .name = "Tunnel",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_ENVIRONMENT,
        .trigger_desc = "Low light environment",
        .state = MOCHI_STATE_COOL,
        .activity = MOCHI_ACTIVITY_IDLE,
        .theme = MOCHI_THEME_LAVENDER,
        .custom_id = 0,
    },

    /*-------------------------------------------------------------------------
     * Category: Parking/Maneuvers (4 states)
     *-----------------------------------------------------------------------*/
    {
        .name = "Parking",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_PARKING,
        .trigger_desc = "Slow back-forth motion",
        .state = MOCHI_STATE_WORRIED,
        .activity = MOCHI_ACTIVITY_WIGGLE,
        .theme = MOCHI_THEME_MINT,
        .custom_id = 0,
    },
    {
        .name = "Parallel Park",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_PARKING,
        .trigger_desc = "Rotation while reversing",
        .state = MOCHI_STATE_WORRIED,
        .activity = MOCHI_ACTIVITY_SPIN,
        .theme = MOCHI_THEME_LAVENDER,
        .custom_id = 0,
    },
    {
        .name = "Parked!",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_PARKING,
        .trigger_desc = "Parking complete",
        .state = MOCHI_STATE_EXCITED,
        .activity = MOCHI_ACTIVITY_BOUNCE,
        .theme = MOCHI_THEME_SAKURA,
        .custom_id = 0,
    },
    {
        .name = "U-Turn",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_PARKING,
        .trigger_desc = "180 degree rotation",
        .state = MOCHI_STATE_DIZZY,
        .activity = MOCHI_ACTIVITY_SPIN,
        .theme = MOCHI_THEME_PEACH,
        .custom_id = 0,
    },

    /*-------------------------------------------------------------------------
     * Category: Safety/Alerts (4 states)
     *-----------------------------------------------------------------------*/
    {
        .name = "Collision!",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_SAFETY,
        .trigger_desc = "Extreme deceleration",
        .state = MOCHI_STATE_PANIC,
        .activity = MOCHI_ACTIVITY_VIBRATE,
        .theme = MOCHI_THEME_PEACH,
        .custom_id = 0,
    },
    {
        .name = "Drowsy Alert",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_SAFETY,
        .trigger_desc = "Late night + static",
        .state = MOCHI_STATE_SLEEPY,
        .activity = MOCHI_ACTIVITY_NOD,
        .theme = MOCHI_THEME_LAVENDER,
        .custom_id = 0,
    },
    {
        .name = "Seatbelt",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_SAFETY,
        .trigger_desc = "Movement started",
        .state = MOCHI_STATE_WORRIED,
        .activity = MOCHI_ACTIVITY_NOD,
        .theme = MOCHI_THEME_PEACH,
        .custom_id = 0,
    },
    {
        .name = "Low Fuel",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_SAFETY,
        .trigger_desc = "Battery < 20%",
        .state = MOCHI_STATE_WORRIED,
        .activity = MOCHI_ACTIVITY_WIGGLE,
        .theme = MOCHI_THEME_PEACH,
        .custom_id = 0,
    },

    /*-------------------------------------------------------------------------
     * Category: Entertainment/Mood (3 states)
     *-----------------------------------------------------------------------*/
    {
        .name = "Music Mode",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_ENTERTAINMENT,
        .trigger_desc = "Rhythmic motion",
        .state = MOCHI_STATE_HAPPY,
        .activity = MOCHI_ACTIVITY_BOUNCE,
        .theme = MOCHI_THEME_SAKURA,
        .custom_id = 0,
    },
    {
        .name = "Road Trip",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_ENTERTAINMENT,
        .trigger_desc = "Extended drive 30+ min",
        .state = MOCHI_STATE_COOL,
        .activity = MOCHI_ACTIVITY_BLINK,
        .theme = MOCHI_THEME_MINT,
        .custom_id = 0,
    },
    {
        .name = "Arrived!",
        .type = ANIM_TYPE_FACE,
        .category = CAR_CAT_ENTERTAINMENT,
        .trigger_desc = "Stop after long drive",
        .state = MOCHI_STATE_EXCITED,
        .activity = MOCHI_ACTIVITY_BOUNCE,
        .theme = MOCHI_THEME_SAKURA,
        .custom_id = 0,
    },

    /*=========================================================================
     * CUSTOM ANIMATIONS (36) - Using gallery_animations rendering
     *=======================================================================*/

    /*-------------------------------------------------------------------------
     * Category: Abstract Geometric (6 animations)
     *-----------------------------------------------------------------------*/
    {
        .name = "Pulsing Rings",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_GEOMETRIC,
        .trigger_desc = "Expanding concentric circles",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_PULSING_RINGS,
    },
    {
        .name = "Spiral Galaxy",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_GEOMETRIC,
        .trigger_desc = "Rotating spiral pattern",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_SPIRAL_GALAXY,
    },
    {
        .name = "Heartbeat",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_GEOMETRIC,
        .trigger_desc = "EKG pulse line",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_HEARTBEAT,
    },
    {
        .name = "Breathing Orb",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_GEOMETRIC,
        .trigger_desc = "Expanding/contracting circle",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_BREATHING_ORB,
    },
    {
        .name = "Matrix Rain",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_GEOMETRIC,
        .trigger_desc = "Digital rain effect",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_MATRIX_RAIN,
    },
    {
        .name = "Radar Sweep",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_GEOMETRIC,
        .trigger_desc = "Rotating radar line",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_RADAR_SWEEP,
    },

    /*-------------------------------------------------------------------------
     * Category: Weather Effects (6 animations)
     *-----------------------------------------------------------------------*/
    {
        .name = "Rain Storm",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_WEATHER,
        .trigger_desc = "Falling blue droplets",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_RAIN_STORM,
    },
    {
        .name = "Snowfall",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_WEATHER,
        .trigger_desc = "Drifting white snowflakes",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_SNOWFALL,
    },
    {
        .name = "Sunshine",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_WEATHER,
        .trigger_desc = "Radiating sun rays",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_SUNSHINE,
    },
    {
        .name = "Lightning",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_WEATHER,
        .trigger_desc = "Electric bolt flash",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_LIGHTNING,
    },
    {
        .name = "Starry Night",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_WEATHER,
        .trigger_desc = "Twinkling stars",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_STARRY_NIGHT,
    },
    {
        .name = "Aurora",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_WEATHER,
        .trigger_desc = "Northern lights waves",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_AURORA,
    },

    /*-------------------------------------------------------------------------
     * Category: Emoji/Symbols (6 animations)
     *-----------------------------------------------------------------------*/
    {
        .name = "Floating Hearts",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_SYMBOLS,
        .trigger_desc = "Rising pink hearts",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_FLOATING_HEARTS,
    },
    {
        .name = "Star Burst",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_SYMBOLS,
        .trigger_desc = "Exploding star pattern",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_STAR_BURST,
    },
    {
        .name = "Question Mark",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_SYMBOLS,
        .trigger_desc = "Bouncing question",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_QUESTION_MARK,
    },
    {
        .name = "Exclamation!",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_SYMBOLS,
        .trigger_desc = "Pulsing alert symbol",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_EXCLAMATION,
    },
    {
        .name = "Checkmark",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_SYMBOLS,
        .trigger_desc = "Success with sparkles",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_CHECKMARK,
    },
    {
        .name = "X Mark",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_SYMBOLS,
        .trigger_desc = "Shaking red X",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_X_MARK,
    },

    /*-------------------------------------------------------------------------
     * Category: Tech/Digital (6 animations)
     *-----------------------------------------------------------------------*/
    {
        .name = "Loading Spinner",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_TECH,
        .trigger_desc = "Rotating dot circle",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_LOADING_SPINNER,
    },
    {
        .name = "Progress Bar",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_TECH,
        .trigger_desc = "Filling progress bar",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_PROGRESS_BAR,
    },
    {
        .name = "Sound Waves",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_TECH,
        .trigger_desc = "Audio visualizer bars",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_SOUND_WAVES,
    },
    {
        .name = "WiFi Signal",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_TECH,
        .trigger_desc = "Animated WiFi arcs",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_WIFI_SIGNAL,
    },
    {
        .name = "Battery Charging",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_TECH,
        .trigger_desc = "Charging battery icon",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_BATTERY_CHARGING,
    },
    {
        .name = "Binary Code",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_TECH,
        .trigger_desc = "Scrolling 0s and 1s",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_BINARY_CODE,
    },

    /*-------------------------------------------------------------------------
     * Category: Nature/Organic (6 animations)
     *-----------------------------------------------------------------------*/
    {
        .name = "Bouncing Ball",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_NATURE,
        .trigger_desc = "Physics bouncing ball",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_BOUNCING_BALL,
    },
    {
        .name = "Ocean Waves",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_NATURE,
        .trigger_desc = "Scrolling sine waves",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_OCEAN_WAVES,
    },
    {
        .name = "Butterfly",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_NATURE,
        .trigger_desc = "Flapping wing butterfly",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_BUTTERFLY,
    },
    {
        .name = "Fireworks",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_NATURE,
        .trigger_desc = "Colorful burst pattern",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_FIREWORKS,
    },
    {
        .name = "Campfire",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_NATURE,
        .trigger_desc = "Flickering flames",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_CAMPFIRE,
    },
    {
        .name = "Bubbles",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_NATURE,
        .trigger_desc = "Rising and popping",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_BUBBLES,
    },

    /*-------------------------------------------------------------------------
     * Category: Dashboard/Automotive (6 animations)
     *-----------------------------------------------------------------------*/
    {
        .name = "Speedometer",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_DASHBOARD,
        .trigger_desc = "Animated speed gauge",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_SPEEDOMETER,
    },
    {
        .name = "Fuel Gauge",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_DASHBOARD,
        .trigger_desc = "Fuel level needle",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_FUEL_GAUGE,
    },
    {
        .name = "Turn Left",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_DASHBOARD,
        .trigger_desc = "Blinking left arrow",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_TURN_LEFT,
    },
    {
        .name = "Turn Right",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_DASHBOARD,
        .trigger_desc = "Blinking right arrow",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_TURN_RIGHT,
    },
    {
        .name = "Hazard Lights",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_DASHBOARD,
        .trigger_desc = "Both arrows blinking",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_HAZARD_LIGHTS,
    },
    {
        .name = "Gear Display",
        .type = ANIM_TYPE_CUSTOM,
        .category = CAR_CAT_DASHBOARD,
        .trigger_desc = "Animated gear shift",
        .state = 0, .activity = 0, .theme = 0,
        .custom_id = GALLERY_ANIM_GEAR_DISPLAY,
    },
};

#define ANIMATION_COUNT (sizeof(s_animations) / sizeof(s_animations[0]))

/*===========================================================================
 * Public API
 *===========================================================================*/

const car_animation_t* car_gallery_get_animations(void)
{
    return s_animations;
}

int car_gallery_get_count(void)
{
    return ANIMATION_COUNT;
}

const char* car_gallery_category_name(car_category_t cat)
{
    if (cat >= CAR_CAT_MAX) {
        return "Unknown";
    }
    return s_category_names[cat];
}
