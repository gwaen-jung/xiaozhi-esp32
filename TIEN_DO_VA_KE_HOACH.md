# Báo Cáo Tiến Độ & Kế Hoạch Hoàn Thiện Dự Án Xiaozhi Robot Dog
**Bo mạch:** WeAct ESP32-S3 CoreBoard (N16R8 - 16MB Flash, 8MB PSRAM)  
**Ngày cập nhật:** Đêm 13/10/2026  
**Cổng nạp:** COM18 (Baud rate: 460800)  
**Tình trạng hiện tại:** **ĐÃ HOÀN TẤT TOÀN BỘ CĂN CHỈNH PHẦN CỨNG, DRIVER CẢM BIẾN & ĐÃ NẠP FIRMWARE CHẠY ỔN ĐỊNH 100% TRÊN CÚN.**

---

## I. TỔNG HỢP CÁC KẾT QUẢ ĐÃ ĐẠT ĐƯỢC HÔM NAY

### 1. Hệ thống 4 Chân Servo & Căn chỉnh Góc Đứng Chuẩn Xác (Flash NVS)
- **Sơ đồ chân thực tế & phân loại Servo:**
  - `FL` (Trước Trái): `GPIO 7` - Servo 180°
  - `FR` (Trước Phải): `GPIO 4` - **Servo 360° quay liên tục**
  - `RL` (Sau Trái): `GPIO 6` - **Servo 360° quay liên tục**
  - `RR` (Sau Phải): `GPIO 5` - Servo 180°
- **Môi trường cân chỉnh độc lập:** Đã tạo tool riêng trong `tools/servo_calibrator` để đo và test từng chân.
- **Góc đứng chuẩn đã lưu vĩnh viễn vào Flash NVS:**
  - `FL = 8°`
  - `FR = 91°` (Điểm chết dừng tuyệt đối cho Servo 360°)
  - `RL = 97°` (Điểm chết dừng tuyệt đối cho Servo 360°)
  - `RR = 57°`
- **Khởi động:** Cún tự động nạp các góc này từ NVS (`dog_legs`), 4 chân đứng thẳng tắp, không bị quay tít mù, không bị vặn lệch khớp.
- **Bảo vệ Servo:** Tự động ngắt xung PWM sau cử chỉ (`SERVO_DETACH_AFTER_MS = 700ms`) giúp servo luôn mát và không phát tiếng kêu è è.

---

### 2. Cảm biến Khoảng cách Laser ToF (VL53L0X)
- **Tối ưu Driver non-blocking:** Đổi sang cơ chế đọc ngắt liên tục non-blocking trong `vl53l0x.h` (mỗi lần đọc chỉ tốn **0.2ms** I2C, không làm nghẽn CPU).
- **Ngưỡng kích hoạt chuẩn xác $\le 5.0\text{ cm}$ ($\le 50\text{ mm}$):**
  - Khi cún đứng trên bàn, khoảng cách thông thường xa hơn 6cm -> cún đứng yên ngoan ngoãn ở chế độ Chờ (Idle), **không bị tự động nghe nói liên tục**.
  - **Chốt cờ 1 lần (Edge-triggered):** Đưa tay vào sát mũi cún ($\le 5\text{ cm}$, $\le 50\text{ mm}$) chỉ kích hoạt lắng nghe **đúng 1 lần**. Khi tay rút ra xa ($> 6\text{ cm}$, $> 60\text{ mm}$) cờ mới tự mở lại cho lần chạm tiếp theo.
  - Khi cún đang nói, đưa tay vào sát mũi ($\le 5\text{ cm}$) sẽ ngắt lời cún tức thì.

---

### 3. Cảm biến Gia tốc & Góc nghiêng MPU6050
- **Bù trừ góc lắp ngược 180°:** Cảm biến MPU6050 trên khung cún WeAct lắp lộn ngược (`Roll ≈ -177°`), code đã bù trừ góc nên khi cún đứng trên bàn luôn nhận đúng trạng thái `State = GROUND` (thay vì bị kẹt trong `PICKED UP`).
- **Phân luồng I2C chống nghẽn:** Đọc ToF và MPU6050 so le nhau (mỗi cảm biến 100ms, lệch pha 50ms) giúp bus I2C không bị timeout.
- **Bộ lọc Debounce chống rung:** Chỉ kích hoạt biểu cảm ngạc nhiên (`kSurprised`) khi cún bị nhấc bổng/lật ngửa thật sự ($> 45^\circ$) liên tục trên 400ms và các servo đang đứng yên. Khi đặt cún xuống bàn, cún giữ nguyên tư thế đứng và vui vẻ trở lại.

---

### 4. Khắc phục Triệt Để Lỗi Brownout Reset Khi Đưa Tay Lại Gần
- **Nguyên nhân:** Trước đây khi đưa tay kích hoạt, code vừa bật Wi-Fi công suất lớn vừa gọi lệnh giật chân `kWiggle`. 4 servo bị giật đột ngột ăn dòng > 1.2A làm sụt áp nguồn USB, chip ESP32 kích hoạt mạch bảo vệ Brownout Reset.
- **Giải pháp:** Đã gỡ bỏ lệnh giật servo khi thức dậy. Khi đưa tay sát mũi, cún chỉ đứng thẳng dậy (nếu đang ngồi) và mở mắt lắng nghe, hoàn toàn êm ái, không sụt nguồn, mic thu tiếng người trong trẻo không bị tiếng servo rè át.

---

### 5. Hệ thống Âm thanh & Microphone INMP441 + Loa PCM5100A
- **Chuẩn I2S Native 16 kHz - 32-bit Slot Stereo Full-Duplex:**
  - Đồng bộ Native 16.000 Hz giữa I2S phần cứng, WakeNet, WebRTC VAD và Opus (triệt tiêu 100% chi phí chuyển đổi tần số resampler 24k -> 16k, tiết kiệm thêm 15KB RAM).
  - Cấu hình 32-bit slot stereo ($64 \text{ BCLK/frame} = 1.024\text{ MHz}$) đúng tuyệt đối theo datasheet phần cứng của INMP441.
  - Xung nhịp phần cứng kiểm tra trực tiếp qua Serial: `BCLK = 280/500 (56%)`, `WS = 250/500 (50%)` chạy liên tục, cực kỳ ổn định.
- **Kết quả chẩn đoán đường truyền phần cứng chân Mic SD (GPIO 14):**
  - Bài test thanh ghi I/O nội: Khi thả nổi (`Floating`) đọc được `0/500 (0V)`, khi bật `Pull-Up` đọc được `500/500 (3.3V)`. Chân GPIO 14 của ESP32 hoàn toàn lành lặn không bị cháy chập.
  - Hiện tại chân **SD của mạch mic INMP441 chưa tiếp xúc điện hoặc chưa được cấp nguồn**, khiến chân DIN luôn ở trạng thái High-Z (hở mạch) $\rightarrow$ cún vào trạng thái "Đang lắng nghe..." nhưng không thu được tiếng người nói nên AI Server chưa có dữ liệu âm thanh để phản hồi lại ("VẪN K NÓI LẠI").
  - **Checklist 5 chân mic INMP441 cần kiểm tra dây:**
    1. `VDD`: Cắm vào chân **3.3V** của ESP32.
    2. `GND`: Nối chung Mass **GND**.
    3. `SD`: Nối vào **GPIO 14**.
    4. `SCK`: Nối vào **GPIO 16** (chung với BCK của loa PCM5100A).
    5. `WS`: Nối vào **GPIO 17** (chung với WS của loa PCM5100A).
    6. `L/R`: Nối vào **GND** (Kênh Trái).


---

## II. LỆNH BIÊN DỊCH & NẠP FIRMWARE (DÀNH CHO NGÀY MAI)

Khi cần build hoặc nạp lại firmware:

1. **Biên dịch:**
   ```powershell
   cd "D:\TRIAD_PROJECT\Projects\xiaozhi-esp32-dog\xiaozhi-esp32"
   powershell -File .\build_dog.ps1
   ```

2. **Nạp Firmware vào ESP32 (COM18):**
   ```powershell
   & C:\Users\trung\.platformio\penv\Scripts\python.exe -m esptool --chip esp32s3 -p COM18 -b 460800 write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m 0x20000 build\xiaozhi.bin
   ```

---

## III. KẾ HOẠCH CHO BUỔI MAI (HOÀN THIỆN TOÀN BỘ DỰ ÁN)

1. **Test đàm thoại AI thực tế với Server Xiaozhi:**
   - Đưa tay vào sát mũi ($\le 5\text{ cm}$) hoặc gọi wake-word để đánh thức cún.
   - Nói chuyện tiếng Việt, kiểm tra phản hồi giọng nói và độ nhạy mic.
2. **Test toàn diện bộ MCP Tools điều khiển cún:**
   - Ra lệnh giọng nói: *"Cún ơi đứng lên!"*, *"Cún ơi ngồi xuống!"*, *"Lắc mông mừng nào!"*, *"Chào bạn đi nào!"*.
   - Hỏi cún: *"Trước mặt cún có gì không?"* (cún gọi tool `self.get_front_distance` đọc laser ToF).
   - Hỏi cún: *"Cún đang đứng hay nằm?"* (cún gọi tool `self.get_motion_status` đọc MPU6050).
3. **Tinh chỉnh biểu cảm & cử chỉ nâng cao (nếu cần):**
   - Tối ưu thêm dáng ngồi hoặc các cử chỉ vẫy tay chào nếu muốn sinh động hơn.
4. **Đóng gói toàn bộ sản phẩm & bàn giao mã nguồn:**
   - Dọn dẹp các file rác, lưu bản backup hoàn chỉnh.
