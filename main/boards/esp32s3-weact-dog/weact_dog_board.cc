// XIAOZHI dog robot on a WeAct ESP32-S3 CoreBoard N16R8:
// ILI9341 eyes, INMP441 mic, I2S amp, 4 servo legs.
#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <esp_lcd_ili9341.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_system.h>

#include "application.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "dog_behavior.h"
#include "dog_config.h"
#include "eyes_display.h"
#include "led/single_led.h"
#include "leg_servos.h"
#include "wifi_board.h"

#define TAG "WeactDogBoard"

// NoAudioCodecDuplex for shared I2S clock (PCM5100A DAC + INMP441 Mic).
class DogAudioCodec : public NoAudioCodecDuplex {
public:
    DogAudioCodec()
        : NoAudioCodecDuplex(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                             PIN_I2S_BCLK, PIN_I2S_WS, PIN_I2S_DOUT, PIN_I2S_DIN) {
        output_volume_ = AUDIO_DEFAULT_OUTPUT_VOLUME;  // NVS value wins if the user changed it
        if (PIN_AMP_SD != GPIO_NUM_NC) {
            gpio_config_t io = {};
            io.pin_bit_mask = 1ULL << PIN_AMP_SD;
            io.mode = GPIO_MODE_OUTPUT;
            gpio_config(&io);
            gpio_set_level(PIN_AMP_SD, 0);  // muted until output is enabled
        }
    }

protected:
    void EnableOutput(bool enable) override {
        if (PIN_AMP_SD != GPIO_NUM_NC && !enable) gpio_set_level(PIN_AMP_SD, 0);
        NoAudioCodecDuplex::EnableOutput(enable);
        if (PIN_AMP_SD != GPIO_NUM_NC && enable) gpio_set_level(PIN_AMP_SD, 1);
    }
};

class WeactDogBoard : public WifiBoard {
private:
    Button boot_button_;
    EyesDisplay* display_ = nullptr;
    LegServos legs_;
    DogBehavior* behavior_ = nullptr;

    static void LogResetReason() {
        const char* name = "unknown";
        switch (esp_reset_reason()) {
            case ESP_RST_POWERON: name = "power-on"; break;
            case ESP_RST_SW: name = "software"; break;
            case ESP_RST_PANIC: name = "panic"; break;
            case ESP_RST_INT_WDT: name = "interrupt watchdog"; break;
            case ESP_RST_TASK_WDT: name = "task watchdog"; break;
            case ESP_RST_WDT: name = "other watchdog"; break;
            case ESP_RST_DEEPSLEEP: name = "deep sleep"; break;
            case ESP_RST_BROWNOUT: name = "BROWNOUT (check battery/boost/servo current)"; break;
            case ESP_RST_USB: name = "usb"; break;
            case ESP_RST_EXT: name = "external pin"; break;
            default: break;
        }
        if (esp_reset_reason() == ESP_RST_BROWNOUT) {
            ESP_LOGE(TAG, "Reset reason: %s", name);
        } else {
            ESP_LOGI(TAG, "Reset reason: %s", name);
        }
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = PIN_TFT_MOSI;
        buscfg.miso_io_num = PIN_TFT_MISO;
        buscfg.sclk_io_num = PIN_TFT_SCLK;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * EYES_PUSH_LINES * sizeof(uint16_t) + 64;
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = PIN_TFT_CS;
        io_config.dc_gpio_num = PIN_TFT_DC;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = DISPLAY_SPI_HZ;
        io_config.trans_queue_depth = 4;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_SPI_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = PIN_TFT_RST;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

        display_ = new EyesDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    }

    void InitializeButtons() {
        // Push-to-talk: a click toggles listening (upstream behaviour).
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

public:
    WeactDogBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        LogResetReason();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
#ifdef CONFIG_DOG_EYES_DEBUG_CYCLE
        display_->StartDebugCycle(2500);  // M2 bench test: cycle moods
#endif
        legs_.Begin();  // task only; servos attach after Wi-Fi + audio are up
        behavior_ = new DogBehavior(display_, &legs_);
        behavior_->Start();
    }

    Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    AudioCodec* GetAudioCodec() override {
        static DogAudioCodec audio_codec;
        return &audio_codec;
    }

    Display* GetDisplay() override { return display_; }
};

DECLARE_BOARD(WeactDogBoard);
