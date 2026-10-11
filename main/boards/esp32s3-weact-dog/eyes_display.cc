#include "eyes_display.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>

#include <cstring>
#include <vector>

#include "dog_config.h"

#define TAG "EyesDisplay"

using namespace roboeyes;

EyesDisplay::EyesDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                         int width, int height)
    : panel_io_(panel_io), panel_(panel), canvas_(width, height), eyes_(canvas_) {
    width_ = width;
    height_ = height;
#if HAVE_LVGL
    // The core's asset loader builds LVGL font/image objects (LVGL builtin TLSF
    // allocator) even though this UI never draws with LVGL: initialise it once.
    if (!lv_is_initialized()) lv_init();
#endif

    mutex_ = xSemaphoreCreateRecursiveMutex();
    dma_free_ = xSemaphoreCreateCounting(2, 2);
    for (auto& b : dma_buf_) {
        b = (uint16_t*)heap_caps_malloc(width * EYES_PUSH_LINES * sizeof(uint16_t),
                                        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if (b == nullptr) ESP_LOGE(TAG, "Push buffer allocation failed");
    }

    esp_lcd_panel_io_callbacks_t cbs = {};
    cbs.on_color_trans_done = &EyesDisplay::OnColorTransDone;
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(panel_io_, &cbs, this));

    canvas_.setBackground(EYES_COLOR_BG);
    canvas_.fillScreen(EYES_COLOR_BG);

    // RoboEyes setup sized for 320x240 (constants in dog_config.h)
    eyes_.begin(width, height, EYES_FPS);
    eyes_.setDisplayColors(EYES_COLOR_BG, EYES_COLOR_MAIN);
    SetEyeSize(EYES_WIDTH, EYES_HEIGHT);
    eyes_.setBorderradius(EYES_RADIUS, EYES_RADIUS);
    eyes_.setSpacebetween(EYES_SPACE);
    eyes_.eyelidsHeightMax = EYES_HEIGHT / 2;
    eyes_.eyelidsHappyBottomOffsetMax = EYES_HEIGHT / 2 + 3;
    // Start centred (the library's member defaults assume 128x64).
    eyes_.setPosition(DEFAULT);
    eyes_.eyeLx = eyes_.eyeLxNext;
    eyes_.eyeLy = eyes_.eyeLyNext;
    eyes_.eyeRx = eyes_.eyeLx + EYES_WIDTH + EYES_SPACE;
    eyes_.eyeRy = eyes_.eyeLy;
    eyes_.setAutoblinker(ON, 3, 3);
    eyes_.sweatScale = EYES_SWEAT_SCALE;

    esp_lcd_panel_disp_on_off(panel_, true);

    xTaskCreatePinnedToCore(&EyesDisplay::RenderTaskEntry, "eyes", EYES_TASK_STACK, this,
                            EYES_TASK_PRIORITY, &task_, EYES_TASK_CORE);
}

EyesDisplay::~EyesDisplay() {
    if (debug_timer_) {
        esp_timer_stop(debug_timer_);
        esp_timer_delete(debug_timer_);
    }
    if (task_) vTaskDelete(task_);
    for (auto& b : dma_buf_) heap_caps_free(b);
    vSemaphoreDelete(dma_free_);
    vSemaphoreDelete(mutex_);
}

bool EyesDisplay::Lock(int timeout_ms) {
    TickType_t ticks = timeout_ms == 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(mutex_, ticks) == pdTRUE;
}

void EyesDisplay::Unlock() { xSemaphoreGiveRecursive(mutex_); }

// ---------------------------------------------------------------------------
// Display interface
// ---------------------------------------------------------------------------
void EyesDisplay::SetupUI() { Display::SetupUI(); }

void EyesDisplay::SetStatus(const char* status) {
    // No status bar on the eyes UI; keep it in the log.
    ESP_LOGI(TAG, "status: %s", status ? status : "");
}

void EyesDisplay::ShowNotification(const char* notification, int duration_ms) {
    if (notification == nullptr) return;
    ESP_LOGI(TAG, "notify: %s", notification);
    DisplayLockGuard lock(this);
    subtitle_ = notification;
    subtitle_until_us_ = esp_timer_get_time() + (int64_t)duration_ms * 1000;
}

void EyesDisplay::SetEmotion(const char* emotion) {
    if (emotion == nullptr) emotion = "neutral";
    ESP_LOGI(TAG, "emotion: %s", emotion);
    if (emotion_hook_) emotion_hook_(emotion);
}

void EyesDisplay::SetChatMessage(const char* role, const char* content) {
    if (role == nullptr) return;
    if (content == nullptr) content = "";
    if (chat_hook_) chat_hook_(role, content);
    // Chat text is dropped on purpose; only "system" messages (activation code,
    // Wi-Fi provisioning hint, errors) are shown as a subtitle.
    if (strcmp(role, "system") == 0) {
        DisplayLockGuard lock(this);
        subtitle_ = content;
        subtitle_until_us_ = 0;
    }
}

void EyesDisplay::ClearChatMessages() {
    DisplayLockGuard lock(this);
    if (subtitle_until_us_ == 0) subtitle_.clear();
}

// ---------------------------------------------------------------------------
// Eyes control
// ---------------------------------------------------------------------------
void EyesDisplay::SetEyeSize(int w, int h) {
    eyes_.setWidth(w, w);
    eyes_.setHeight(h, h);
}

void EyesDisplay::SetCuriosity(bool on) {
    DisplayLockGuard lock(this);
    eyes_.setCuriosity(on);
}

void EyesDisplay::SetSweat(bool on) {
    DisplayLockGuard lock(this);
    eyes_.setSweat(on);
}

void EyesDisplay::ApplyPreset(EyePreset p) {
    DisplayLockGuard lock(this);
    // Common reset for "where to look / how big"
    auto normal = [this]() {
        SetEyeSize(EYES_WIDTH, EYES_HEIGHT);
        eyes_.setCuriosity(OFF);
        eyes_.setHFlicker(OFF);
        eyes_.setVFlicker(OFF);
        eyes_.setAutoblinker(ON, 3, 3);
    };
    if (p != EyePreset::kError && p != EyePreset::kSpeaking) eyes_.setSweat(OFF);

    switch (p) {
        case EyePreset::kBoot:
            normal();
            eyes_.setMood(DEFAULT);
            eyes_.close();  // start closed, the library opens them smoothly
            eyes_.open();
            eyes_.setIdleMode(OFF);
            eyes_.setPosition(DEFAULT);
            break;
        case EyePreset::kIdle:
            normal();
            eyes_.setMood(DEFAULT);
            eyes_.setIdleMode(ON, 2, 3);
            break;
        case EyePreset::kNeutral:
            normal();
            eyes_.setMood(DEFAULT);
            eyes_.setIdleMode(OFF);
            eyes_.setPosition(DEFAULT);
            break;
        case EyePreset::kListening:
            normal();
            SetEyeSize(EYES_WIDE_WIDTH, EYES_WIDE_HEIGHT);
            eyes_.setMood(DEFAULT);
            eyes_.setIdleMode(OFF);
            eyes_.setPosition(DEFAULT);
            eyes_.setCuriosity(ON);
            break;
        case EyePreset::kThinking:
            normal();
            eyes_.setMood(DEFAULT);
            eyes_.setIdleMode(OFF);
            eyes_.setPosition(NW);
            eyes_.setAutoblinker(ON, 4, 3);
            break;
        case EyePreset::kSpeaking:
            // Keep the mood set by the latest server emotion.
            SetEyeSize(EYES_WIDTH, EYES_HEIGHT);
            eyes_.setCuriosity(OFF);
            eyes_.setIdleMode(OFF);
            eyes_.setPosition(DEFAULT);
            eyes_.setAutoblinker(ON, 3, 3);
            break;
        case EyePreset::kHappy:
            normal();
            eyes_.setMood(HAPPY);
            break;
        case EyePreset::kLaugh:
            normal();
            eyes_.setMood(HAPPY);
            eyes_.anim_laugh();
            break;
        case EyePreset::kSad:
            normal();
            eyes_.setMood(TIRED);
            break;
        case EyePreset::kAngry:
            normal();
            eyes_.setMood(ANGRY);
            break;
        case EyePreset::kSurprised:
            normal();
            SetEyeSize(EYES_WIDE_WIDTH, EYES_WIDE_HEIGHT);
            eyes_.setMood(DEFAULT);
            eyes_.setCuriosity(ON);
            break;
        case EyePreset::kConfused:
            normal();
            eyes_.setMood(DEFAULT);
            eyes_.anim_confused();
            break;
        case EyePreset::kSleepy:
            normal();
            eyes_.setMood(TIRED);
            eyes_.setAutoblinker(ON, 6, 4);
            break;
        case EyePreset::kError:
            normal();
            eyes_.setMood(DEFAULT);
            eyes_.setSweat(ON);
            break;
    }
}

void EyesDisplay::StartDebugCycle(int period_ms) {
    if (debug_timer_ != nullptr) return;
    esp_timer_create_args_t args = {};
    args.callback = [](void* arg) {
        auto* self = static_cast<EyesDisplay*>(arg);
        static const EyePreset order[] = {
            EyePreset::kIdle,  EyePreset::kHappy,    EyePreset::kLaugh,    EyePreset::kSad,
            EyePreset::kAngry, EyePreset::kSurprised, EyePreset::kConfused, EyePreset::kSleepy,
            EyePreset::kError, EyePreset::kListening, EyePreset::kThinking,
        };
        int i = self->debug_index_++ % (int)(sizeof(order) / sizeof(order[0]));
        ESP_LOGI(TAG, "debug preset %d, %.1f fps", i, self->fps_);
        self->ApplyPreset(order[i]);
    };
    args.arg = this;
    args.name = "eyes_debug";
    esp_timer_create(&args, &debug_timer_);
    esp_timer_start_periodic(debug_timer_, (uint64_t)period_ms * 1000);
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
void EyesDisplay::RenderTaskEntry(void* arg) { static_cast<EyesDisplay*>(arg)->RenderLoop(); }

void EyesDisplay::DrawSubtitle() {
    if (subtitle_until_us_ != 0 && esp_timer_get_time() > subtitle_until_us_) {
        subtitle_.clear();
        subtitle_until_us_ = 0;
    }
    if (subtitle_.empty()) return;

    // Split into UTF-8 code points, wrap by character count.
    std::vector<std::string> cps;
    for (size_t i = 0; i < subtitle_.size();) {
        unsigned char c = subtitle_[i];
        size_t n = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
        if (c == '\n') {
            cps.emplace_back(" ");
            i++;
            continue;
        }
        cps.emplace_back(subtitle_.substr(i, n));
        i += n;
    }
    int scale = EYES_TEXT_SCALE;
    int per_line = width_ / (Rgb565Canvas::glyphW() * scale);
    int max_lines = EYES_TEXT_MAX_LINES;
    if ((int)cps.size() > per_line * max_lines) {  // too long: use the small font
        scale = 1;
        per_line = width_ / Rgb565Canvas::glyphW();
        max_lines = EYES_TEXT_MAX_LINES * 2;
    }
    std::vector<std::string> lines;
    std::string line;
    int count = 0;
    for (auto& cp : cps) {
        line += cp;
        if (++count == per_line) {
            lines.push_back(line);
            line.clear();
            count = 0;
        }
    }
    if (!line.empty()) lines.push_back(line);
    // Keep the tail: the activation code is at the end of the message.
    size_t first = lines.size() > (size_t)max_lines ? lines.size() - max_lines : 0;
    int lh = Rgb565Canvas::glyphH() * scale;
    int nlines = (int)(lines.size() - first);
    int y = height_ - nlines * lh - 2;
    canvas_.fillRect(0, y - 2, width_, nlines * lh + 4, EYES_COLOR_BG);
    for (size_t i = first; i < lines.size(); ++i, y += lh) {
        int w = Rgb565Canvas::textWidth(lines[i].c_str(), scale);
        canvas_.drawText((width_ - w) / 2, y, lines[i].c_str(), EYES_COLOR_TEXT, scale);
    }
}

bool EyesDisplay::OnColorTransDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*,
                                   void* user_ctx) {
    auto* self = static_cast<EyesDisplay*>(user_ctx);
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(self->dma_free_, &woken);
    return woken == pdTRUE;
}

void EyesDisplay::Push(const DirtyBox& box) {
    if (box.empty() || dma_buf_[0] == nullptr || dma_buf_[1] == nullptr) return;
    const uint16_t* fb = canvas_.buffer();
    int w = box.x1 - box.x0 + 1;
    int lines_per_chunk = (width_ * EYES_PUSH_LINES) / w;
    for (int y = box.y0; y <= box.y1; y += lines_per_chunk) {
        int ye = std::min(y + lines_per_chunk - 1, box.y1);
        xSemaphoreTake(dma_free_, portMAX_DELAY);
        uint16_t* dst = dma_buf_[dma_next_];
        dma_next_ ^= 1;
        for (int yy = y; yy <= ye; ++yy) {
            memcpy(dst + (yy - y) * w, fb + (size_t)yy * width_ + box.x0, w * sizeof(uint16_t));
        }
        if (esp_lcd_panel_draw_bitmap(panel_, box.x0, y, box.x1 + 1, ye + 1, dst) != ESP_OK) {
            xSemaphoreGive(dma_free_);  // no transfer queued, no callback will come
        }
    }
}

void EyesDisplay::RenderLoop() {
    const TickType_t period = pdMS_TO_TICKS(1000 / EYES_FPS);
    TickType_t last = xTaskGetTickCount();
    int frames = 0;
    int64_t fps_t0 = esp_timer_get_time();
    if (!canvas_.ok()) {
        ESP_LOGE(TAG, "No frame buffer, eyes disabled");
        vTaskDelete(nullptr);
    }
    while (true) {
        DirtyBox box;
        if (Lock(1000)) {
            eyes_.drawEyes();
            DrawSubtitle();
            box = canvas_.takeDirty();
            Unlock();
        }
        Push(box);

        if (++frames == 60) {
            int64_t now = esp_timer_get_time();
            fps_ = 60.0f * 1e6f / (float)(now - fps_t0);
            fps_t0 = now;
            frames = 0;
        }
        // If a frame overran, do not try to catch up (no busy loop).
        if (xTaskGetTickCount() - last >= period) last = xTaskGetTickCount() - period + 1;
        vTaskDelayUntil(&last, period);
    }
}
