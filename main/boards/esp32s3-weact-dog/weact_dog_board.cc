// XIAOZHI dog robot on a WeAct ESP32-S3 CoreBoard N16R8:
// ILI9341 eyes, INMP441 mic, I2S amp, 4 servo legs.
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_ili9341.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_rom_gpio.h>
#include <soc/gpio_sig_map.h>

#include "application.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "dog_behavior.h"
#include "dog_config.h"
#include "eyes_display.h"
#include "led/single_led.h"
#include "leg_servos.h"
#include "mcp_server.h"
#include "mpu6050.h"
#include "vl53l0x.h"
#include "wifi_board.h"

#define TAG "WeactDogBoard"

#include <cmath>
#include <vector>

// Stereo 32-bit Full-Duplex I2S Codec for PCM5100A DAC + INMP441 Mic.
// PCM5100A internal PLL requires standard stereo 50% duty-cycle WS and 64xFs BCK.
class DogAudioCodec : public NoAudioCodec {
public:
    DogAudioCodec() {
        duplex_ = true;
        input_sample_rate_ = AUDIO_INPUT_SAMPLE_RATE;
        output_sample_rate_ = AUDIO_OUTPUT_SAMPLE_RATE;

        // Ensure pins can be read by software gpio_get_level diagnostics
        gpio_input_enable(PIN_I2S_BCLK);
        gpio_input_enable(PIN_I2S_WS);
        gpio_input_enable(PIN_I2S_DIN);

        // Hardware Line Test for DIN (GPIO 14) to detect short-circuit or floating line
        gpio_reset_pin(PIN_I2S_DIN);
        gpio_set_direction(PIN_I2S_DIN, GPIO_MODE_INPUT);
        gpio_pullup_en(PIN_I2S_DIN);
        gpio_pulldown_dis(PIN_I2S_DIN);
        vTaskDelay(pdMS_TO_TICKS(10));
        int din_pu = gpio_get_level(PIN_I2S_DIN);

        gpio_pulldown_en(PIN_I2S_DIN);
        gpio_pullup_dis(PIN_I2S_DIN);
        vTaskDelay(pdMS_TO_TICKS(10));
        int din_pd = gpio_get_level(PIN_I2S_DIN);

        gpio_pullup_dis(PIN_I2S_DIN);
        gpio_pulldown_dis(PIN_I2S_DIN);
        ESP_LOGI(TAG, "DIN (IO%d) Hardware Line Test: PullUp=%d, PullDown=%d", PIN_I2S_DIN, din_pu, din_pd);

        i2s_chan_config_t chan_cfg = {
            .id = I2S_NUM_0,
            .role = I2S_ROLE_MASTER,
            .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
            .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
            .auto_clear_after_cb = true,
            .auto_clear_before_cb = false,
            .intr_priority = 0,
        };
        ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

        i2s_std_config_t std_cfg = {
            .clk_cfg = {
                .sample_rate_hz = (uint32_t)output_sample_rate_,
                .clk_src = I2S_CLK_SRC_DEFAULT,
#if SOC_I2S_HW_VERSION_2
                .ext_clk_freq_hz = 0,
#endif
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            },
            .slot_cfg = {
                .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
                .slot_mode = I2S_SLOT_MODE_STEREO,
                .slot_mask = I2S_STD_SLOT_BOTH,
                .ws_width = 32,
                .ws_pol = false,
                .bit_shift = true,
#if SOC_I2S_HW_VERSION_2
                .left_align = true,
                .big_endian = false,
                .bit_order_lsb = false,
#endif
            },
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = PIN_I2S_BCLK,
                .ws = PIN_I2S_WS,
                .dout = PIN_I2S_DOUT,
                .din = PIN_I2S_DIN,
                .invert_flags = {
                    .mclk_inv = false,
                    .bclk_inv = false,
                    .ws_inv = false,
                },
            },
        };
        // Initialize TX FIRST as Master clock generator
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
        // Initialize RX SECOND as Full-Duplex Slave (shares TX BCLK & WS internally)
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));

        // Start BOTH channels immediately and keep them running continuously via DMA.
        // With auto_clear_after_cb=true, TX outputs silent zeroes when idle while
        // maintaining the 64-clock BCLK/WS frame (1.024 MHz) strictly required by INMP441.
        ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
        ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));

        output_volume_ = AUDIO_DEFAULT_OUTPUT_VOLUME;
        if (static_cast<int>(PIN_AMP_SD) >= 0) {
            gpio_config_t io = {};
            int pin = static_cast<int>(PIN_AMP_SD);
            io.pin_bit_mask = (pin >= 0) ? (1ULL << pin) : 0ULL;
            io.mode = GPIO_MODE_OUTPUT;
            gpio_config(&io);
            gpio_set_level(PIN_AMP_SD, 1);
        }
        ESP_LOGI(TAG, "PCM5100A + INMP441 32-bit Slot Stereo Duplex ready (64 BCLK/frame = 1.024 MHz)");
    }

    ~DogAudioCodec() override {
        if (tx_handle_ != nullptr) {
            i2s_channel_disable(tx_handle_);
        }
        if (rx_handle_ != nullptr) {
            i2s_channel_disable(rx_handle_);
        }
    }

    int Write(const int16_t* data, int samples) override {
        std::lock_guard<std::mutex> lock(data_if_mutex_);
        if (tx_buf32_.size() < (size_t)(samples * 2)) {
            tx_buf32_.resize(samples * 2);
        }

        int32_t volume_factor = output_volume_; // 0..100
        for (int i = 0; i < samples; i++) {
            int32_t val = (int32_t(data[i]) * volume_factor) / 100;
            // Place 16-bit audio in upper 16 bits of 32-bit slot for PCM5100A DAC
            int32_t sample32 = val << 16;
            tx_buf32_[i * 2]     = sample32;
            tx_buf32_[i * 2 + 1] = sample32;
        }

        size_t bytes_written;
        ESP_ERROR_CHECK(i2s_channel_write(tx_handle_, tx_buf32_.data(), samples * 2 * sizeof(int32_t), &bytes_written, portMAX_DELAY));
        return bytes_written / (2 * sizeof(int32_t));
    }

    int Read(int16_t* dest, int samples) override {
        size_t bytes_read = 0;
        constexpr uint32_t kReadTimeoutMs = 100;

        if (rx_buf32_.size() < (size_t)(samples * 2)) {
            rx_buf32_.resize(samples * 2);
        }
        if (i2s_channel_read(rx_handle_, rx_buf32_.data(), samples * 2 * sizeof(int32_t), &bytes_read, kReadTimeoutMs) != ESP_OK) {
            return 0;
        }

        int frames = bytes_read / (2 * sizeof(int32_t));
        int32_t max_l = 0, max_r = 0;
        for (int i = 0; i < frames; i++) {
            // INMP441 outputs 24 bits placed at bits [31:8] in 32-bit slot.
            // Right shift by 14 preserves sign, maps to 16-bit and provides +12dB digital gain.
            int32_t l_val = rx_buf32_[i * 2] >> 14;
            int32_t r_val = rx_buf32_[i * 2 + 1] >> 14;
            int16_t s_l = (l_val > INT16_MAX) ? INT16_MAX : (l_val < INT16_MIN) ? INT16_MIN : (int16_t)l_val;
            int16_t s_r = (r_val > INT16_MAX) ? INT16_MAX : (r_val < INT16_MIN) ? INT16_MIN : (int16_t)r_val;
            int32_t al = std::abs((int32_t)s_l);
            int32_t ar = std::abs((int32_t)s_r);
            if (al > max_l) max_l = al;
            if (ar > max_r) max_r = ar;
        }

        static int active_channel = 0; // 0=Left, 1=Right
        if (max_l > max_r * 2 && max_l > 50) {
            active_channel = 0;
        } else if (max_r > max_l * 2 && max_r > 50) {
            active_channel = 1;
        }

        for (int i = 0; i < frames; i++) {
            int32_t raw = (active_channel == 0) ? rx_buf32_[i * 2] : rx_buf32_[i * 2 + 1];
            int32_t shifted = raw >> 14;
            dest[i] = (shifted > INT16_MAX) ? INT16_MAX : (shifted < INT16_MIN) ? INT16_MIN : (int16_t)shifted;
        }

        // Periodic diagnostic telemetry for mic input (every 2 seconds)
        static int64_t last_mic_log = 0;
        int64_t now = esp_timer_get_time() / 1000;
        if (now - last_mic_log > 2000) {
            last_mic_log = now;
            int b_hi = 0, w_hi = 0, d_hi = 0;
            for (int k = 0; k < 500; k++) {
                if (gpio_get_level(PIN_I2S_BCLK)) b_hi++;
                if (gpio_get_level(PIN_I2S_WS)) w_hi++;
                if (gpio_get_level(PIN_I2S_DIN)) d_hi++;
            }
            ESP_LOGI(TAG, "[Mic Diagnostic] frames=%d, raw0=0x%08lX, raw1=0x%08lX, maxL=%ld, maxR=%ld (BCLK=%d/500, WS=%d/500, DIN=%d/500, ch=%s)",
                     frames, (unsigned long)rx_buf32_[0], (unsigned long)rx_buf32_[1],
                     (long)max_l, (long)max_r, b_hi, w_hi, d_hi, (active_channel == 0) ? "LEFT" : "RIGHT");
        }

        return frames;
    }

protected:
    std::vector<int32_t> rx_buf32_;
    std::vector<int32_t> tx_buf32_;


    void EnableInput(bool enable) override {
        std::lock_guard<std::mutex> lock(data_if_mutex_);
        AudioCodec::EnableInput(enable);
    }

    void EnableOutput(bool enable) override {
        std::lock_guard<std::mutex> lock(data_if_mutex_);
        if (enable == output_enabled_) return;
        if (static_cast<int>(PIN_AMP_SD) >= 0) {
            gpio_set_level(PIN_AMP_SD, enable ? 1 : 0);
        }
        AudioCodec::EnableOutput(enable);
    }
};

class WeactDogBoard : public WifiBoard {
private:
    Button boot_button_;
    EyesDisplay* display_ = nullptr;
    LegServos legs_;
    DogBehavior* behavior_ = nullptr;
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    VL53L0X* tof_sensor_ = nullptr;
    MPU6050* mpu6050_ = nullptr;

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

    void InitializeI2c() {
        // Diagnostic check of I2C bus pins (GPIO 8 = SDA, GPIO 9 = SCL)
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << PIN_I2C_SDA) | (1ULL << PIN_I2C_SCL);
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&io_conf);
        vTaskDelay(pdMS_TO_TICKS(10));

        int sda_lvl = gpio_get_level(PIN_I2C_SDA);
        int scl_lvl = gpio_get_level(PIN_I2C_SCL);
        ESP_LOGI(TAG, "I2C pin diagnostics: SDA (IO%d)=%d, SCL (IO%d)=%d",
                 PIN_I2C_SDA, sda_lvl, PIN_I2C_SCL, scl_lvl);

        // If SDA is stuck low while SCL is high, clock SCL 9 times to un-stick any slave
        if (sda_lvl == 0 && scl_lvl == 1) {
            ESP_LOGW(TAG, "SDA line stuck LOW! Performing I2C bus recovery (9 clock pulses on SCL)...");
            gpio_set_direction(PIN_I2C_SCL, GPIO_MODE_OUTPUT);
            for (int i = 0; i < 9; ++i) {
                gpio_set_level(PIN_I2C_SCL, 0);
                esp_rom_delay_us(10);
                gpio_set_level(PIN_I2C_SCL, 1);
                esp_rom_delay_us(10);
            }
            gpio_set_direction(PIN_I2C_SCL, GPIO_MODE_INPUT);
            vTaskDelay(pdMS_TO_TICKS(5));
            sda_lvl = gpio_get_level(PIN_I2C_SDA);
            scl_lvl = gpio_get_level(PIN_I2C_SCL);
            ESP_LOGI(TAG, "After bus recovery: SDA=%d, SCL=%d", sda_lvl, scl_lvl);
        }

        if (sda_lvl == 0 || scl_lvl == 0) {
            ESP_LOGW(TAG, "I2C bus line held LOW! Please verify: 1) Common GND; 2) Sensor VCC 3.3V; 3) SDA/SCL not swapped.");
        }

        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = PIN_I2C_SDA,
            .scl_io_num = PIN_I2C_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        esp_err_t err = i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "I2C master bus initialized on SDA=%d, SCL=%d", PIN_I2C_SDA, PIN_I2C_SCL);
            
            // Full I2C scan to discover ALL responding addresses
            ESP_LOGI(TAG, "--- Quet toan bo thiet bi I2C tren bus ---");
            int dev_count = 0;
            for (uint8_t a = 0x08; a < 0x78; ++a) {
                if (i2c_master_probe(i2c_bus_, a, 20) == ESP_OK) {
                    ESP_LOGI(TAG, ">>> Tim thay thiet bi I2C tai dia chi: 0x%02X", a);
                    dev_count++;
                }
            }
            if (dev_count == 0) {
                ESP_LOGW(TAG, "Khong tim thay thiet bi I2C nao phan hoi!");
            } else {
                ESP_LOGI(TAG, "Tong cong tim thay %d thiet bi I2C tren bus.", dev_count);
            }

            // 1. ToF Laser distance sensor (VL53L0X / VL53L1X)
            tof_sensor_ = new VL53L0X(i2c_bus_);
            if (!tof_sensor_->Init()) {
                ESP_LOGW(TAG, "ToF distance sensor init failed or sensor not responding at 0x29");
            }

            // 2. MPU6050 6-Axis Gyro & Accelerometer
            mpu6050_ = new MPU6050(i2c_bus_);
            if (!mpu6050_->Init()) {
                ESP_LOGW(TAG, "MPU6050 init failed or sensor not responding at 0x68");
            }
        } else {
            ESP_LOGE(TAG, "Failed to initialize I2C master bus: %s", esp_err_to_name(err));
        }
    }

    void InitializeMcpTools() {
        auto& mcp_server = McpServer::GetInstance();

        // 1. Tool to control robot dog legs (Cún Xiaozhi Persona)
        mcp_server.AddTool(
            "self.control_dog_legs",
            "Bạn là Cún robot Xiaozhi thông minh có 4 chân servo thực tế. "
            "Khi người dùng bảo bạn đứng, ngồi, nằm, lắc mông, vẫy đuôi, vẫy chân chào hay nghỉ ngơi, "
            "bạn BẮT BUỘC phải gọi công cụ này để cử động và phản hồi hào hứng như một chú cún đáng yêu.\n"
            "Các hành động (action):\n"
            "- 'stand': Đứng thẳng lên 4 chân\n"
            "- 'sit': Ngồi ngoan xuống\n"
            "- 'crouch': Nằm rạp / cúi mình xuống\n"
            "- 'wiggle': Lắc mông, vẫy đuôi vui mừng\n"
            "- 'wave': Giơ chân trước vẫy chào / bắt tay\n"
            "- 'relax': Thả lỏng 4 chân nghỉ ngơi.",
            PropertyList({Property("action", kPropertyTypeString)}),
            [this](const PropertyList& properties) -> ToolResult {
                auto action = properties["action"].value<std::string>();
                ESP_LOGI(TAG, "MCP control_dog_legs called with action: %s", action.c_str());

                if (action == "stand" || action == "dung" || action == "dung_len") {
                    legs_.Play(LegMotion::kStand);
                    return true;
                } else if (action == "sit" || action == "ngoi" || action == "ngoi_xuong") {
                    legs_.Play(LegMotion::kSit);
                    return true;
                } else if (action == "crouch" || action == "nam" || action == "cui") {
                    legs_.Play(LegMotion::kCrouch);
                    return true;
                } else if (action == "wiggle" || action == "lac" || action == "lac_mong") {
                    legs_.Play(LegMotion::kWiggle);
                    return true;
                } else if (action == "wave" || action == "vay_chan" || action == "chao") {
                    legs_.Play(LegMotion::kWave);
                    return true;
                } else if (action == "relax") {
                    legs_.Play(LegMotion::kRelax);
                    return true;
                } else if (action == "zero" || action == "0" || action == "ve_0") {
                    legs_.Play(LegMotion::kZero);
                    return true;
                } else if (action == "save" || action == "luu" || action == "luu_goc") {
                    legs_.SaveStandPose();
                    return true;
                } else if (action == "reset_pose" || action == "khoi_phuc") {
                    legs_.ResetStandPose();
                    return true;
                }
                return std::unexpected("Hành động không hợp lệ: " + action);
            });

        // 1b. Tool to set single leg angle (tinh chỉnh từng chân 0..180 độ)
        mcp_server.AddTool(
            "self.set_leg_angle",
            "Điều khiển góc quay của một chân servo cụ thể (0 đến 180 độ). "
            "Chân gồm: 'FL' (trước trái), 'FR' (trước phải - servo 360°), 'RL' (sau trái), 'RR' (sau phải).",
            PropertyList({Property("leg", kPropertyTypeString), Property("angle", kPropertyTypeInteger)}),
            [this](const PropertyList& properties) -> ToolResult {
                auto leg_str = properties["leg"].value<std::string>();
                int angle = properties["angle"].value<int>();
                int leg_idx = -1;
                if (leg_str == "FL" || leg_str == "fl" || leg_str == "truoc_trai") leg_idx = 0;
                else if (leg_str == "FR" || leg_str == "fr" || leg_str == "truoc_phai") leg_idx = 1;
                else if (leg_str == "RL" || leg_str == "rl" || leg_str == "sau_trai") leg_idx = 2;
                else if (leg_str == "RR" || leg_str == "rr" || leg_str == "sau_phai") leg_idx = 3;
                else return std::unexpected("Tên chân không hợp lệ (hãy dùng FL, FR, RL, RR): " + leg_str);

                legs_.SetLegAngle(leg_idx, (float)angle);
                char buf[64];
                snprintf(buf, sizeof(buf), "Đã chỉnh chân %s về góc %d độ", leg_str.c_str(), angle);
                return std::string(buf);
            });

        // 1c. Tool to save current leg angles as default stand pose in Flash NVS
        mcp_server.AddTool(
            "self.save_stand_pose",
            "Lưu góc đứng hiện tại của 4 chân vào bộ nhớ Flash NVS vĩnh viễn. "
            "Sau khi gọi công cụ này, mỗi lần cún khởi động lại máy sẽ tự động đứng theo đúng các góc này mà không bị vặn lệch.",
            PropertyList(),
            [this](const PropertyList& /*properties*/) -> ToolResult {
                legs_.SaveStandPose();
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "Đã lưu góc đứng thành công vào Flash NVS! FL=%d°, FR=%d°, RL=%d°, RR=%d°. Từ nay khi khởi động lại, cún sẽ luôn đứng theo góc này.",
                         legs_.GetStandAngle(0), legs_.GetStandAngle(1), legs_.GetStandAngle(2), legs_.GetStandAngle(3));
                return std::string(buf);
            });

        // 1d. Tool to get current and saved stand leg angles
        mcp_server.AddTool(
            "self.get_leg_angles",
            "Xem góc hiện tại và góc đứng đã lưu trong Flash NVS của 4 chân (FL, FR, RL, RR).",
            PropertyList(),
            [this](const PropertyList& /*properties*/) -> ToolResult {
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "Góc hiện tại: FL=%.0f°, FR=%.0f°, RL=%.0f°, RR=%.0f° | Góc đứng đã lưu: FL=%d°, FR=%d°, RL=%d°, RR=%d°",
                         legs_.GetLegAngle(0), legs_.GetLegAngle(1), legs_.GetLegAngle(2), legs_.GetLegAngle(3),
                         legs_.GetStandAngle(0), legs_.GetStandAngle(1), legs_.GetStandAngle(2), legs_.GetStandAngle(3));
                return std::string(buf);
            });

        // 2. Tool to get distance from VL53L0X
        mcp_server.AddTool(
            "self.get_front_distance",
            "Đo khoảng cách vật cản phía trước mặt cún (tính bằng cm) bằng cảm biến laser ToF VL53L0X. "
            "Hãy gọi công cụ này khi người dùng hỏi phía trước có gì, khoảng cách bao xa, hoặc khi cún muốn quan sát.",
            PropertyList(),
            [this](const PropertyList& /*properties*/) -> ToolResult {
                if (tof_sensor_ == nullptr || !tof_sensor_->IsInitialized()) {
                    return std::string("Cảm biến khoảng cách VL53L0X chưa được kết nối.");
                }
                uint16_t dist_mm = tof_sensor_->ReadDistanceMm();
                if (dist_mm == 0 || dist_mm >= 8000) {
                    return std::string("Phía trước thông thoáng, không có vật cản gần (khoảng cách > 2 mét).");
                }
                float dist_cm = dist_mm / 10.0f;
                char buf[64];
                snprintf(buf, sizeof(buf), "Khoảng cách vật cản phía trước: %.1f cm (%d mm)", dist_cm, dist_mm);
                return std::string(buf);
            });

        // 3. Tool to get motion/tilt/pickup status from MPU6050
        mcp_server.AddTool(
            "self.get_motion_status",
            "Đọc trạng thái tư thế, độ nghiêng (pitch, roll) và kiểm tra cún có đang bị bế lên / nhấc bổng hay không từ cảm biến MPU6050. "
            "Hãy gọi công cụ này khi được hỏi bạn đang đứng hay nằm, hoặc bạn cảm thấy mình đang bị bế lên.",
            PropertyList(),
            [this](const PropertyList& /*properties*/) -> ToolResult {
                if (mpu6050_ == nullptr || !mpu6050_->IsInitialized()) {
                    return std::string("Cảm biến gia tốc/góc nghiêng MPU6050 chưa được kết nối.");
                }
                MPU6050::MotionData data;
                if (!mpu6050_->ReadMotion(&data)) {
                    return std::string("Không đọc được dữ liệu từ MPU6050.");
                }
                std::string status = data.is_picked_up ? "Đang bị bế lên hoặc nghiêng mạnh!" : "Đang đứng yên ổn định.";
                char buf[160];
                snprintf(buf, sizeof(buf), "Pitch: %.1f deg, Roll: %.1f deg. %s", data.pitch_deg, data.roll_deg, status.c_str());
                return std::string(buf);
            });

        ESP_LOGI(TAG, "Registered Dog MCP tools: self.control_dog_legs, self.get_front_distance, self.get_motion_status");
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
        InitializeI2c();
        InitializeMcpTools();
#ifdef CONFIG_DOG_EYES_DEBUG_CYCLE
        display_->StartDebugCycle(2500);  // M2 bench test: cycle moods
#endif
        legs_.Begin();  // task only; servos attach after Wi-Fi + audio are up
        behavior_ = new DogBehavior(display_, &legs_);
        if (tof_sensor_ != nullptr) {
            behavior_->SetDistanceSensor(tof_sensor_);
        }
        if (mpu6050_ != nullptr) {
            behavior_->SetMotionSensor(mpu6050_);
        }
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
