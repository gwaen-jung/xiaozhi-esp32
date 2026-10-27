#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// Full-Duplex I2S Configuration (INMP441 Mic + MAX98357A Amp)
#define AUDIO_I2S_GPIO_BCLK     GPIO_NUM_11
#define AUDIO_I2S_GPIO_WS       GPIO_NUM_12
#define AUDIO_I2S_GPIO_DOUT     GPIO_NUM_13
#define AUDIO_I2S_GPIO_DIN      GPIO_NUM_14

#define AUDIO_DEFAULT_OUTPUT_VOLUME 90

// Buttons & LED
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define BUILTIN_LED_GPIO        GPIO_NUM_48
#define TOUCH_BUTTON_GPIO       GPIO_NUM_NC
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

// OLED SSD1306 Display (I2C: SDA=GPIO 1, SCL=GPIO 2, Addr=0x3C)
#define DISPLAY_SDA_PIN         GPIO_NUM_1
#define DISPLAY_SCL_PIN         GPIO_NUM_2
#define DISPLAY_WIDTH           128
#define DISPLAY_HEIGHT          64
#define DISPLAY_MIRROR_X        false
#define DISPLAY_MIRROR_Y        false

// Mini L298N DC Motors (Dual H-Bridge)
#define PIN_MOTOR_A_IN1         GPIO_NUM_4
#define PIN_MOTOR_A_IN2         GPIO_NUM_5
#define PIN_MOTOR_B_IN3         GPIO_NUM_6
#define PIN_MOTOR_B_IN4         GPIO_NUM_7

#endif // _BOARD_CONFIG_H_
