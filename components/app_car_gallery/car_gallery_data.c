/**
 * @file car_gallery_data.c
 * @brief Car Animation Gallery - Animation catalog (32 car states)
 *
 * Defines all car-themed animations with their mochi states, activities,
 * themes, and trigger descriptions.
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
    "All",           /* CAR_CAT_ALL */
};

/*===========================================================================
 * Animation Catalog (32 States)
 *===========================================================================*/

static const car_animation_t s_animations[] = {
    /*-------------------------------------------------------------------------
     * Category: Engine/Startup (4 states)
     *-----------------------------------------------------------------------*/
    {
        .name = "Ignition",
        .state = MOCHI_STATE_EXCITED,
        .activity = MOCHI_ACTIVITY_BOUNCE,
        .theme = MOCHI_THEME_PEACH,
        .trigger_desc = "Engine starts / power on",
        .category = CAR_CAT_ENGINE,
    },
    {
        .name = "Engine Idle",
        .state = MOCHI_STATE_HAPPY,
        .activity = MOCHI_ACTIVITY_IDLE,
        .theme = MOCHI_THEME_MINT,
        .trigger_desc = "Subtle vibration detected",
        .category = CAR_CAT_ENGINE,
    },
    {
        .name = "Warming Up",
        .state = MOCHI_STATE_SLEEPY,
        .activity = MOCHI_ACTIVITY_SHAKE,
        .theme = MOCHI_THEME_CLOUD,
        .trigger_desc = "Cold temperature",
        .category = CAR_CAT_ENGINE,
    },
    {
        .name = "Engine Off",
        .state = MOCHI_STATE_SLEEPY,
        .activity = MOCHI_ACTIVITY_SNORE,
        .theme = MOCHI_THEME_LAVENDER,
        .trigger_desc = "Complete stillness",
        .category = CAR_CAT_ENGINE,
    },

    /*-------------------------------------------------------------------------
     * Category: Driving Dynamics (8 states)
     *-----------------------------------------------------------------------*/
    {
        .name = "Cruising",
        .state = MOCHI_STATE_COOL,
        .activity = MOCHI_ACTIVITY_IDLE,
        .theme = MOCHI_THEME_CLOUD,
        .trigger_desc = "Steady forward motion",
        .category = CAR_CAT_DRIVING,
    },
    {
        .name = "Accelerating",
        .state = MOCHI_STATE_EXCITED,
        .activity = MOCHI_ACTIVITY_SLIDE_DOWN,
        .theme = MOCHI_THEME_PEACH,
        .trigger_desc = "Strong acceleration",
        .category = CAR_CAT_DRIVING,
    },
    {
        .name = "Hard Braking",
        .state = MOCHI_STATE_SHOCKED,
        .activity = MOCHI_ACTIVITY_SLIDE_UP,
        .theme = MOCHI_THEME_PEACH,
        .trigger_desc = "Rapid deceleration",
        .category = CAR_CAT_DRIVING,
    },
    {
        .name = "Left Turn",
        .state = MOCHI_STATE_HAPPY,
        .activity = MOCHI_ACTIVITY_SLIDE_LEFT,
        .theme = MOCHI_THEME_MINT,
        .trigger_desc = "Roll < 55 degrees",
        .category = CAR_CAT_DRIVING,
    },
    {
        .name = "Right Turn",
        .state = MOCHI_STATE_HAPPY,
        .activity = MOCHI_ACTIVITY_SLIDE_RIGHT,
        .theme = MOCHI_THEME_MINT,
        .trigger_desc = "Roll > 125 degrees",
        .category = CAR_CAT_DRIVING,
    },
    {
        .name = "Reversing",
        .state = MOCHI_STATE_WORRIED,
        .activity = MOCHI_ACTIVITY_WIGGLE,
        .theme = MOCHI_THEME_LAVENDER,
        .trigger_desc = "Pitch < -20 degrees",
        .category = CAR_CAT_DRIVING,
    },
    {
        .name = "Uphill",
        .state = MOCHI_STATE_EXCITED,
        .activity = MOCHI_ACTIVITY_NOD,
        .theme = MOCHI_THEME_PEACH,
        .trigger_desc = "Pitch > 15 degrees",
        .category = CAR_CAT_DRIVING,
    },
    {
        .name = "Downhill",
        .state = MOCHI_STATE_EXCITED,
        .activity = MOCHI_ACTIVITY_SLIDE_DOWN,
        .theme = MOCHI_THEME_CLOUD,
        .trigger_desc = "Pitch < -15 degrees",
        .category = CAR_CAT_DRIVING,
    },

    /*-------------------------------------------------------------------------
     * Category: Speed Zones (4 states)
     *-----------------------------------------------------------------------*/
    {
        .name = "Slow Zone",
        .state = MOCHI_STATE_HAPPY,
        .activity = MOCHI_ACTIVITY_IDLE,
        .theme = MOCHI_THEME_SAKURA,
        .trigger_desc = "Very slow motion",
        .category = CAR_CAT_SPEED,
    },
    {
        .name = "Highway Speed",
        .state = MOCHI_STATE_COOL,
        .activity = MOCHI_ACTIVITY_BLINK,
        .theme = MOCHI_THEME_CLOUD,
        .trigger_desc = "Sustained high speed",
        .category = CAR_CAT_SPEED,
    },
    {
        .name = "Speeding",
        .state = MOCHI_STATE_PANIC,
        .activity = MOCHI_ACTIVITY_VIBRATE,
        .theme = MOCHI_THEME_PEACH,
        .trigger_desc = "Excessive speed warning",
        .category = CAR_CAT_SPEED,
    },
    {
        .name = "Traffic Jam",
        .state = MOCHI_STATE_WORRIED,
        .activity = MOCHI_ACTIVITY_IDLE,
        .theme = MOCHI_THEME_LAVENDER,
        .trigger_desc = "Stop-and-go pattern",
        .category = CAR_CAT_SPEED,
    },

    /*-------------------------------------------------------------------------
     * Category: Environment/Weather (5 states)
     *-----------------------------------------------------------------------*/
    {
        .name = "Sunny Drive",
        .state = MOCHI_STATE_HAPPY,
        .activity = MOCHI_ACTIVITY_BOUNCE,
        .theme = MOCHI_THEME_PEACH,
        .trigger_desc = "Daytime hours",
        .category = CAR_CAT_ENVIRONMENT,
    },
    {
        .name = "Night Drive",
        .state = MOCHI_STATE_COOL,
        .activity = MOCHI_ACTIVITY_BLINK,
        .theme = MOCHI_THEME_LAVENDER,
        .trigger_desc = "Nighttime driving",
        .category = CAR_CAT_ENVIRONMENT,
    },
    {
        .name = "Rain Detected",
        .state = MOCHI_STATE_WORRIED,
        .activity = MOCHI_ACTIVITY_SHAKE,
        .theme = MOCHI_THEME_CLOUD,
        .trigger_desc = "Vibration pattern",
        .category = CAR_CAT_ENVIRONMENT,
    },
    {
        .name = "Bumpy Road",
        .state = MOCHI_STATE_DIZZY,
        .activity = MOCHI_ACTIVITY_SHAKE,
        .theme = MOCHI_THEME_PEACH,
        .trigger_desc = "Irregular vibrations",
        .category = CAR_CAT_ENVIRONMENT,
    },
    {
        .name = "Tunnel",
        .state = MOCHI_STATE_COOL,
        .activity = MOCHI_ACTIVITY_IDLE,
        .theme = MOCHI_THEME_LAVENDER,
        .trigger_desc = "Low light environment",
        .category = CAR_CAT_ENVIRONMENT,
    },

    /*-------------------------------------------------------------------------
     * Category: Parking/Maneuvers (4 states)
     *-----------------------------------------------------------------------*/
    {
        .name = "Parking",
        .state = MOCHI_STATE_WORRIED,
        .activity = MOCHI_ACTIVITY_WIGGLE,
        .theme = MOCHI_THEME_MINT,
        .trigger_desc = "Slow back-forth motion",
        .category = CAR_CAT_PARKING,
    },
    {
        .name = "Parallel Park",
        .state = MOCHI_STATE_WORRIED,
        .activity = MOCHI_ACTIVITY_SPIN,
        .theme = MOCHI_THEME_LAVENDER,
        .trigger_desc = "Rotation while reversing",
        .category = CAR_CAT_PARKING,
    },
    {
        .name = "Parked!",
        .state = MOCHI_STATE_EXCITED,
        .activity = MOCHI_ACTIVITY_BOUNCE,
        .theme = MOCHI_THEME_SAKURA,
        .trigger_desc = "Parking complete",
        .category = CAR_CAT_PARKING,
    },
    {
        .name = "U-Turn",
        .state = MOCHI_STATE_DIZZY,
        .activity = MOCHI_ACTIVITY_SPIN,
        .theme = MOCHI_THEME_PEACH,
        .trigger_desc = "180 degree rotation",
        .category = CAR_CAT_PARKING,
    },

    /*-------------------------------------------------------------------------
     * Category: Safety/Alerts (4 states)
     *-----------------------------------------------------------------------*/
    {
        .name = "Collision!",
        .state = MOCHI_STATE_PANIC,
        .activity = MOCHI_ACTIVITY_VIBRATE,
        .theme = MOCHI_THEME_PEACH,
        .trigger_desc = "Extreme deceleration",
        .category = CAR_CAT_SAFETY,
    },
    {
        .name = "Drowsy Alert",
        .state = MOCHI_STATE_SLEEPY,
        .activity = MOCHI_ACTIVITY_NOD,
        .theme = MOCHI_THEME_LAVENDER,
        .trigger_desc = "Late night + static",
        .category = CAR_CAT_SAFETY,
    },
    {
        .name = "Seatbelt",
        .state = MOCHI_STATE_WORRIED,
        .activity = MOCHI_ACTIVITY_NOD,
        .theme = MOCHI_THEME_PEACH,
        .trigger_desc = "Movement started",
        .category = CAR_CAT_SAFETY,
    },
    {
        .name = "Low Fuel",
        .state = MOCHI_STATE_WORRIED,
        .activity = MOCHI_ACTIVITY_WIGGLE,
        .theme = MOCHI_THEME_PEACH,
        .trigger_desc = "Battery < 20%",
        .category = CAR_CAT_SAFETY,
    },

    /*-------------------------------------------------------------------------
     * Category: Entertainment/Mood (3 states)
     *-----------------------------------------------------------------------*/
    {
        .name = "Music Mode",
        .state = MOCHI_STATE_HAPPY,
        .activity = MOCHI_ACTIVITY_BOUNCE,
        .theme = MOCHI_THEME_SAKURA,
        .trigger_desc = "Rhythmic motion",
        .category = CAR_CAT_ENTERTAINMENT,
    },
    {
        .name = "Road Trip",
        .state = MOCHI_STATE_COOL,
        .activity = MOCHI_ACTIVITY_BLINK,
        .theme = MOCHI_THEME_MINT,
        .trigger_desc = "Extended drive 30+ min",
        .category = CAR_CAT_ENTERTAINMENT,
    },
    {
        .name = "Arrived!",
        .state = MOCHI_STATE_EXCITED,
        .activity = MOCHI_ACTIVITY_BOUNCE,
        .theme = MOCHI_THEME_SAKURA,
        .trigger_desc = "Stop after long drive",
        .category = CAR_CAT_ENTERTAINMENT,
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
