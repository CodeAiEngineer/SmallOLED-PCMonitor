/*
 * SmallOLED-PCMonitor - Clock Animation Constants
 *
 * Named constants for clock animation values.
 * These values are tuned for specific animation behaviors.
 */

#ifndef CLOCK_CONSTANTS_H
#define CLOCK_CONSTANTS_H

// ========== Mario Clock Constants ==========
// Starting position (off-screen left)
#define MARIO_START_X -15

// Walking speed (pixels per frame at MARIO_ANIM_SPEED)
#define MARIO_WALK_SPEED 2.0f

// Velocity after hitting a digit (bounce upward)
#define MARIO_BOUNCE_VELOCITY 2.0f

// Second trigger threshold for animation
#define MARIO_ANIMATION_TRIGGER_SECOND 55

// Mario idle encounter settings (v1.5.2)
#define ENCOUNTER_FREQ_RARE_MIN 25000      // 25s minimum
#define ENCOUNTER_FREQ_RARE_MAX 35000      // 35s maximum
#define ENCOUNTER_FREQ_NORMAL_MIN 15000    // 15s minimum
#define ENCOUNTER_FREQ_NORMAL_MAX 25000    // 25s maximum
#define ENCOUNTER_FREQ_FREQUENT_MIN 8000   // 8s minimum
#define ENCOUNTER_FREQ_FREQUENT_MAX 15000  // 15s maximum
#define ENCOUNTER_FREQ_CHAOTIC_MIN 2000    // 2s minimum
#define ENCOUNTER_FREQ_CHAOTIC_MAX 5000    // 5s maximum
#define ENCOUNTER_SPEED_SLOW 40            // Slow encounter speed
#define ENCOUNTER_SPEED_NORMAL 80          // Normal encounter speed
#define ENCOUNTER_SPEED_FAST 120           // Fast encounter speed
#define ENCOUNTER_AUTO_ABORT_SECOND 56     // Abort encounters at :56s
#define ENCOUNTER_ELEMENT_Y 52             // Ground level for encounter elements
#define ENCOUNTER_ELEMENT_WIDTH 8          // Sprite width
#define GOOMBA_TYPE 1
#define KOOPA_TYPE 2

// ========== Space Clock Constants ==========
// Laser offset from character top (where laser starts)
#define SPACE_LASER_OFFSET_Y 4

// Explosion frames before moving to next target
#define SPACE_EXPLOSION_FRAMES 5

// ========== Digit Positioning ==========
// Standard digit X positions (18px spacing, starting at 19)
#define DIGIT_SPACING_PX 18
#define DIGIT_START_X 19

// ========== Common Values ==========
// Movement threshold (considered "at target" when within this distance)
#define MOVEMENT_THRESHOLD 1.0f

// Walk direction proximity threshold (within 3 pixels = at target)
#define MARIO_TARGET_PROXIMITY 3

// Date display width calculation (for centering)
#define DATE_DISPLAY_WIDTH 60

// Screen center X position
#define SCREEN_CENTER_X 64

#endif // CLOCK_CONSTANTS_H
