// Tunable constants for the eyes and the servo legs. No magic numbers elsewhere.
#pragma once

#include <cstdint>

#include "board_pins.h"

// ============================================================================
// Eyes
// ============================================================================
// Colours are plain RGB565 (0xRRRR RGGG GGGB BBBB). The canvas swaps bytes itself.
#define EYES_COLOR_MAIN       0x07FF  // cyan
#define EYES_COLOR_BG         0x0000  // black
#define EYES_COLOR_TEXT       0xFFFF  // subtitle text (notifications, activation code)

#define EYES_FPS              30      // target frame rate (25..30)
#define EYES_TASK_PRIORITY    2       // low: must never starve audio
#define EYES_TASK_STACK       4096
#define EYES_TASK_CORE        1       // tskNO_AFFINITY is also fine

#define EYES_WIDTH            90
#define EYES_HEIGHT           80
#define EYES_RADIUS           20
#define EYES_SPACE            40
// "Wide open" variant used for listening / surprised
#define EYES_WIDE_WIDTH       96
#define EYES_WIDE_HEIGHT      104

#define EYES_SWEAT_SCALE      3.0f    // sweat drops were sized for 128x64

// Push buffer: internal DMA RAM, two ping-pong chunks of N lines each
#define EYES_PUSH_LINES       24

// Bottom subtitle strip (ASCII + Vietnamese 8x16 font, scaled)
#define EYES_TEXT_SCALE       2
#define EYES_TEXT_MAX_LINES   2

// ============================================================================
// Servo legs
// ============================================================================
enum LegId : uint8_t { LEG_FL = 0, LEG_FR = 1, LEG_RL = 2, LEG_RR = 3, LEG_COUNT = 4 };

struct LegConfig {
    gpio_num_t gpio;
    int8_t trim_deg;  // added to the logical angle
    bool invert;      // mirror around 90 deg (for the right-hand side usually)
    bool is_continuous_360; // true = 360 continuous rotation servo (stop at 90 deg)
};

// TODO(hw-verify): trims and directions per leg after assembly.
// Logical convention: angle > 90 moves the leg "forward/down" (standing taller).
static constexpr LegConfig kLegConfig[LEG_COUNT] = {
    {PIN_SERVO_FL, 0, false, false}, // 180° front-left (GPIO 7)
    {PIN_SERVO_FR, 0, true,  true},  // 360° continuous rotation front-right (GPIO 4)
    {PIN_SERVO_RL, 0, false, true},  // 360° continuous rotation rear-left (GPIO 6)
    {PIN_SERVO_RR, 0, true,  false}, // 180° rear-right (GPIO 5)
};

#define SERVO_STAND_FL_DEG       8     // Chân trước trái (180°)
#define SERVO_STAND_FR_DEG       91    // Chân trước phải (360° deadband stop)
#define SERVO_STAND_RL_DEG       97    // Chân sau trái (360° deadband stop)
#define SERVO_STAND_RR_DEG       57    // Chân sau phải (180°)

#define SERVO_NEUTRAL_DEG        90
#define SERVO_MIN_DEG            0     // cho phép về 0 độ
#define SERVO_MAX_DEG            180   // cho phép lên tối đa 180 độ
#define SERVO_PULSE_MIN_US       500   // pulse at 0 deg
#define SERVO_PULSE_MAX_US       2400  // pulse at 180 deg
#define SERVO_PWM_HZ             50
#define SERVO_PWM_BITS           14
#define SERVO_LEDC_TIMER         LEDC_TIMER_1    // timer 0 is left for backlight
#define SERVO_LEDC_FIRST_CHANNEL LEDC_CHANNEL_2  // channels 2..5

#define SERVO_SLEW_DEG_PER_S     90    // ramp speed limit
#define SERVO_MAX_MOVING         2     // never more than 2 servos moving at once
#define SERVO_STAGGER_MS         80    // delay between two servo start-ups
#define SERVO_DETACH_AFTER_MS    700   // stop PWM after 700ms holding to prevent heat/brownout
#define SERVO_TICK_MS            20    // control loop period
#define SERVO_INIT_GAP_MS        300   // sequential attach at boot
#define SERVO_TASK_PRIORITY      3
#define SERVO_TASK_STACK         4096

#define DOG_IDLE_SIT_AFTER_MS    60000 // idle this long -> sit, then PWM off
#define DOG_SPEAK_WIGGLE_EVERY_MS 3500 // soft wiggle cadence while speaking
#define DOG_SURPRISE_MS          1500  // how long "surprised" curiosity lasts
#define DOG_POLL_MS              50    // state polling period
