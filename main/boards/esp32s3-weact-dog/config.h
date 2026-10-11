#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include "board_pins.h"

// Audio: INMP441 at 16 kHz in, server TTS is 24 kHz Opus out.
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
// Conservative default for small 4 ohm speakers (0..100, changeable by voice).
#define AUDIO_DEFAULT_OUTPUT_VOLUME 60
// TODO(hw-verify): 1 = drive both I2S slots (dual speaker amp), 0 = left only.
#define AUDIO_SPK_STEREO_DUP 1

#define BOOT_BUTTON_GPIO PIN_BUTTON_TALK
#define BUILTIN_LED_GPIO PIN_STATUS_LED

// Display: ILI9341 in landscape.
#define DISPLAY_WIDTH    320
#define DISPLAY_HEIGHT   240
#define DISPLAY_SPI_HOST SPI2_HOST
#define DISPLAY_SPI_HZ   (40 * 1000 * 1000)  // drop to 26/20 MHz if unstable
#define DISPLAY_SPI_MODE 0
// TODO(hw-verify): orientation / colour order / inversion for this module.
#define DISPLAY_SWAP_XY      true
#define DISPLAY_MIRROR_X     false  // swap+no mirror = Adafruit rotation 1 (MADCTL 0x28)
#define DISPLAY_MIRROR_Y     false  // set both true for rotation 3 (upside down)
#define DISPLAY_INVERT_COLOR false
#define DISPLAY_RGB_ORDER    LCD_RGB_ELEMENT_ORDER_BGR

#endif  // _BOARD_CONFIG_H_
