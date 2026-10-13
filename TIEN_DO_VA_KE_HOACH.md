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
  - **Chốt cờ 1 lần (Edge-triggered):** Đưa tay vào sát mũi cún ($\le 5\text{ cm}$, $\le 50\text{ mm}$) chỉ kích hoạt lắng nghe **đúng 1 lần**. Khi tay rút ra xa ($> 7\text{ cm}$) cờ mới tự mở lại cho lần chạm tiếp theo.
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

### 5. Hệ thống Âm thanh & Màn hình RoboEyes
- **Loa PCM5100A & Mic INMP441:** Full-duplex 32-bit Stereo Duplex (`BCK = 64 * Fs`). Đồng bộ tần số lấy mẫu phần cứng 24 kHz cho cả TX/RX; kích hoạt tự động bộ chuyển đổi âm thanh (Audio Resampler 24kHz -> 16kHz) của ESP-ADF cho AFE/VAD nhận diện giọng nói cực chuẩn.
- **Tự động nhận diện kênh Mic INMP441:** Hỗ trợ cả 2 trường hợp chân L/R nối GND (Left slot) hoặc nối 3.3V/treo (Right slot), kèm bộ khuếch đại gain số +12dB và log chẩn đoán biên độ âm thanh thời gian thực.
- **Màn hình mắt RoboEyes (ST7789/ILI9341):** Đổi cảm xúc theo trạng thái máy (Chờ, Kết nối, Lắng nghe, Suy nghĩ, Nói, Vui, Ngạc nhiên).

---

## II. LỆNH BIÊN DỊCH & NẠP FIRMWARE (DÀNH CHO NGÀY MAI)

Khi cần build hoặc nạp lại firmware:

1. **Biên dịch:**
   ```powershell
   cd "c:\Users\trung\Downloads\xiaozhi-esp32-dog\xiaozhi-esp32"
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
