#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/oled_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "motor_controller.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

#define TAG "CompanionRobotBoard"

class CompanionRobotBoard : public WifiBoard {
private:
    Button boot_button_;
    Display* display_ = nullptr;
    i2c_master_bus_handle_t display_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    MotorController motors_;

    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeSsd1306Display() {
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .scl_speed_hz = 400 * 1000,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(display_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 OLED driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
        ESP_LOGI(TAG, "SSD1306 driver installed");

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize OLED SSD1306 display");
            display_ = new NoDisplay();
            return;
        }
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, false));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        boot_button_.OnLongPress([this]() {
            ESP_LOGI(TAG, "BOOT button held -> Entering Wi-Fi config mode");
            EnterWifiConfigMode();
        });
    }

    void InitializeMcpTools() {
        motors_.Initialize();
        auto& mcp_server = McpServer::GetInstance();

        // 1. Tool move_forward
        mcp_server.AddTool(
            "self.move_forward",
            "Điều khiển robot chạy tiến về phía trước một đoạn ngắn.",
            PropertyList(),
            [this](const PropertyList& /*properties*/) -> ToolResult {
                motors_.MoveForward(200);
                vTaskDelay(pdMS_TO_TICKS(600));
                motors_.Stop();
                return std::string("Robot đã chạy tiến về phía trước.");
            });

        // 2. Tool move_backward
        mcp_server.AddTool(
            "self.move_backward",
            "Điều khiển robot chạy lùi về phía sau một đoạn ngắn.",
            PropertyList(),
            [this](const PropertyList& /*properties*/) -> ToolResult {
                motors_.MoveBackward(200);
                vTaskDelay(pdMS_TO_TICKS(600));
                motors_.Stop();
                return std::string("Robot đã lùi lại một đoạn.");
            });

        // 3. Tool turn_left
        mcp_server.AddTool(
            "self.turn_left",
            "Điều khiển robot quay bánh xe sang trái.",
            PropertyList(),
            [this](const PropertyList& /*properties*/) -> ToolResult {
                motors_.TurnLeft(190);
                vTaskDelay(pdMS_TO_TICKS(350));
                motors_.Stop();
                return std::string("Robot đã quay sang trái.");
            });

        // 4. Tool turn_right
        mcp_server.AddTool(
            "self.turn_right",
            "Điều khiển robot quay bánh xe sang phải.",
            PropertyList(),
            [this](const PropertyList& /*properties*/) -> ToolResult {
                motors_.TurnRight(190);
                vTaskDelay(pdMS_TO_TICKS(350));
                motors_.Stop();
                return std::string("Robot đã quay sang phải.");
            });

        // 5. Tool dance
        mcp_server.AddTool(
            "self.dance",
            "Robot nhảy múa lắc lư 2 bánh xe vui vẻ theo điệu nhạc.",
            PropertyList(),
            [this](const PropertyList& /*properties*/) -> ToolResult {
                motors_.WiggleDance(4);
                return std::string("Robot vừa nhảy múa lắc lư siêu vui nhộn!");
            });

        // 6. Tool stop
        mcp_server.AddTool(
            "self.stop",
            "Dừng ngay lập tức toàn bộ động cơ.",
            PropertyList(),
            [this](const PropertyList& /*properties*/) -> ToolResult {
                motors_.Stop();
                return std::string("Đã dừng xe an toàn.");
            });

        ESP_LOGI(TAG, "Registered Robot MCP tools: self.move_forward, self.move_backward, self.turn_left, self.turn_right, self.dance, self.stop");
    }

public:
    CompanionRobotBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeDisplayI2c();
        InitializeSsd1306Display();
        InitializeButtons();
        InitializeMcpTools();
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecDuplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN
        );
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(CompanionRobotBoard);
