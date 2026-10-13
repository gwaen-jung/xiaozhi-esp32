# Tiến Độ & Kế Hoạch Dự Án Xiaozhi Robot Dog (WeAct ESP32-S3 CoreBoard N16R8)

**Ngày cập nhật:** 13/10/2026 (Buổi tối)  
**Trạng thái chung:** **ĐÃ HOÀN THÀNH TOÀN BỘ CÁC MỤC KẾ HOẠCH & ĐÃ NẠP FIRMWARE MỚI THÀNH CÔNG VÀO ESP32 (COM18)**.

---

## I. Những Gì Đã Hoàn Thành

### 1. Âm thanh Loa ngoài (PCM5100A DAC + INMP441 Mic) - Hoạt động hoàn hảo:
- Cấu hình full-duplex I2S 32-bit Stereo chuẩn (`BCK = 64 * Fs`, WS đối xứng 50% duty cycle) đáp ứng chuẩn PLL nội của PCM5100A.
- Nhân bản (duplicate) luồng âm thanh thoại mono sang cả kênh Left và Right, âm lượng to, rõ ràng, không rè, mic đàm thoại 2 chiều nhạy.

### 2. Tối ưu Hệ Thống 4 Chân Servo & Xử lý Servo 360°:
- **Ánh xạ sơ đồ chân thực tế:**
  - `FL` (Trước Trái): `GPIO 7` (Servo 180°)
  - `FR` (Trước Phải): `GPIO 4` (**Servo 360° quay liên tục**)
  - `RL` (Sau Trái): `GPIO 6` (Servo 180°)
  - `RR` (Sau Phải): `GPIO 5` (Servo 180°)
- **Cơ chế giữ vững cho servo 360° (FR):** Được gắn cờ `is_continuous_360`, luôn khóa ở **deadband trung tính 90° (+ trim)** để đứng vững làm trụ, không bao giờ bị quay tít mù làm hỏng khớp hay xoắn dây.
- **Cơ chế chống nóng & chống brownout:** Rút ngắn thời gian ngắt xung PWM xuống **700ms** (`SERVO_DETACH_AFTER_MS`). Sau khi thực hiện xong cử chỉ (đứng, ngồi, nằm, vẫy), xung PWM tự ngắt để servo hoàn toàn nguội và êm ái.

### 3. Cảm biến Khoảng cách ToF Laser & Cảm biến Góc nghiêng MPU6050:
- **Tự động phục hồi Bus I2C (Bus Recovery):** Thêm cơ chế kiểm tra mức logic chân SDA (`IO8`) và SCL (`IO9`). Nếu bị kẹt mức LOW, firmware tự động phát 9 xung SCL để giải phóng thiết bị ngoại vi bị treo.
- **Chống spam timeout:** Trong `vl53l0x.h`, nếu gặp lỗi timeout (khi cảm biến chưa cắm hoặc chưa nối GND), vòng lặp quét dừng ngay lập tức thay vì quét 112 lần làm đơ hệ thống lúc boot.
- **Độ nhạy cử chỉ:** Nâng khoảng cách đưa tay sát cún để tự động lắng nghe từ 1.5 cm lên **5.0 cm** (50 mm) trong `dog_behavior.cc`.

### 4. Tùy biến Persona "Cún Xiaozhi" (Dog Xiaozhi):
- Cập nhật mô tả MCP Tools trực tiếp trong firmware (`self.control_dog_legs`, `self.get_front_distance`, `self.get_motion_status`) với ngôn ngữ chỉ dẫn tiếng Việt chi tiết, hướng dẫn AI nhận biết mình có 4 chân, sủa "Gâu gâu!" và chủ động gọi tool khi nghe lệnh.
- Soạn thảo tài liệu [DOG_XIAOZHI_PROMPT.md](file:///c:/Users/trung/Downloads/xiaozhi-esp32-dog/xiaozhi-esp32/DOG_XIAOZHI_PROMPT.md) chứa System Prompt đầy đủ và bảng khẩu lệnh tiếng Việt.

### 5. Biên dịch & Nạp Code:
- Toàn bộ firmware đã được biên dịch thành công 100% bằng `build_dog.ps1`.
- Đã nạp thành công 100% vào chip ESP32-S3 qua cổng **COM18** bằng `flash_dog.ps1`.

---

## II. Hướng Dẫn Vận Hành & Trải Nghiệm Thực Tế

1. **Khởi động cún:** Nhấn nút **RST** trên bo WeAct ESP32-S3 để cún bắt đầu chạy firmware mới.
2. **Quan sát:**
   - Màn hình mắt mở ra chớp mắt vui vẻ.
   - 4 chân khởi động tuần tự và đứng vững, sau 700ms servo êm ru không nóng.
3. **Tương tác đưa tay sát mặt cún (< 5cm):**
   - Đưa lòng bàn tay lại gần mũi cún trong khoảng 5cm: mắt chuyển hình vui vẻ, cún lắc mông và bắt đầu lắng nghe bạn nói.
4. **Thử các khẩu lệnh tiếng Việt:**
   - *"Cún ơi đứng lên!"* -> Cún đứng thẳng 4 chân.
   - *"Ngồi xuống ngoan nào!"* -> Cún hạ 2 chân sau, ngồi thẳng 2 chân trước.
   - *"Lắc mông mừng đi!"* -> Cún lắc mông vẫy đuôi.
   - *"Chào bạn đi nào!"* -> Cún giơ chân trước vẫy chào.
   - *"Trước mặt cún có gì không?"* -> Cún đo khoảng cách laser ToF và báo cáo.
   - *"Cún đang đứng hay nằm?"* -> Cún đọc cảm biến MPU6050 báo góc nghiêng.
