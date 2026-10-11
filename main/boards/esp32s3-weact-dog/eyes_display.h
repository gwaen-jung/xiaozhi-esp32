// EyesDisplay: replaces the stock LVGL chat UI with animated RoboEyes on an
// SPI RGB565 panel. Rendering runs in its own low-priority task.
#pragma once

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <functional>
#include <string>

#include "display/display.h"
#include "rgb565_canvas.h"
#include "roboeyes_port.h"

// High-level looks. The mapping from device state / server emotion to these
// presets lives in dog_behavior.cc (one table).
enum class EyePreset : uint8_t {
    kBoot,       // open from closed, DEFAULT
    kIdle,       // DEFAULT, autoblink, idle drift
    kListening,  // wide, centred, curiosity on
    kThinking,   // look up-left, slow drift
    kSpeaking,   // keep current mood, centred, autoblink
    kNeutral,    // DEFAULT, centred, autoblink (no drift)
    kHappy,      // HAPPY
    kLaugh,      // HAPPY + anim_laugh()
    kSad,        // TIRED (droopy)
    kAngry,      // ANGRY
    kSurprised,  // wide + curiosity (owner turns it off after a while)
    kConfused,   // anim_confused()
    kSleepy,     // TIRED + slow blink
    kError,      // sweat on, DEFAULT
};

class EyesDisplay : public Display {
public:
    // Hook so the board's behaviour controller sees what the core sends.
    using EmotionHook = std::function<void(const char* emotion)>;
    using ChatHook = std::function<void(const char* role, const char* content)>;

    EyesDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                int height);
    ~EyesDisplay() override;

    // ---- Display interface ------------------------------------------------
    void SetStatus(const char* status) override;
    void ShowNotification(const char* notification, int duration_ms = 3000) override;
    void ShowNotification(const std::string& notification, int duration_ms = 3000) override {
        ShowNotification(notification.c_str(), duration_ms);
    }
    void SetEmotion(const char* emotion) override;
    void SetChatMessage(const char* role, const char* content) override;
    void ClearChatMessages() override;
    void SetupUI() override;

    // ---- Eyes control (thread-safe) ---------------------------------------
    void ApplyPreset(EyePreset preset);
    void SetCuriosity(bool on);
    void SetSweat(bool on);
    void SetEmotionHook(EmotionHook hook) { emotion_hook_ = std::move(hook); }
    void SetChatHook(ChatHook hook) { chat_hook_ = std::move(hook); }

    // Debug: cycle through all presets on a timer (milestone M2 check).
    void StartDebugCycle(int period_ms);
    float measured_fps() const { return fps_; }

protected:
    bool Lock(int timeout_ms = 0) override;
    void Unlock() override;

private:
    static void RenderTaskEntry(void* arg);
    void RenderLoop();
    void DrawSubtitle();
    void Push(const DirtyBox& box);
    void SetEyeSize(int w, int h);
    static bool OnColorTransDone(esp_lcd_panel_io_handle_t io,
                                 esp_lcd_panel_io_event_data_t* edata, void* user_ctx);

    esp_lcd_panel_io_handle_t panel_io_;
    esp_lcd_panel_handle_t panel_;
    Rgb565Canvas canvas_;
    roboeyes::RoboEyes<Rgb565Canvas> eyes_;

    SemaphoreHandle_t mutex_ = nullptr;      // recursive: guards eyes_, canvas_, text
    SemaphoreHandle_t dma_free_ = nullptr;   // counting: free push buffers
    uint16_t* dma_buf_[2] = {nullptr, nullptr};
    int dma_next_ = 0;
    TaskHandle_t task_ = nullptr;

    std::string subtitle_;
    int64_t subtitle_until_us_ = 0;  // 0 = no expiry
    bool subtitle_drawn_ = false;
    bool sweat_locked_ = false;      // error preset keeps sweat until a non-error preset

    EmotionHook emotion_hook_;
    ChatHook chat_hook_;
    esp_timer_handle_t debug_timer_ = nullptr;
    int debug_index_ = 0;
    volatile float fps_ = 0;
};
