# Tiến Độ & Kế Hoạch Dự Án Xiaozhi Robot Dog (WeAct ESP32-S3 CoreBoard N16R8)

**Ngày cập nhật:** 13/10/2026  
**Trạng thái chung:** Phần cứng audio và khung phần mềm cốt lõi đã hoạt động ổn định. Đã tích hợp các MCP Server Tools, driver màn hình mắt ILI9341, I2S DAC stereo PCM5100A, driver ToF kép (VL53L0X / VL53L1X) và driver MPU6050.

---

## I. Những Gì Đã Hoàn Thành Hôm Nay

1. **Âm thanh Loa ngoài (PCM5100A DAC + INMP441 Mic) - ĐÃ TEST HOÀN HẢO:**
   - Cấu hình full-duplex I2S 32-bit Stereo chuẩn (BCK = 64 * Fs, WS đối xứng 50% duty cycle) đáp ứng chuẩn PLL nội của PCM5100A.
   - Nhân bản (duplicate) luồng âm thanh thoại mono sang cả kênh Left và Right, âm lượng to, rõ ràng, không rè, mic đàm thoại 2 chiều nhạy.

2. **Hệ thống điều khiển Chân Servo (4 Servos SG90/MG90S):**
   - Đã ánh xạ và kiểm soát 4 chân qua GPIO: FL (GPIO6), FR (GPIO7), RL (GPIO4), RR (GPIO5).
   - Đã tạo MCP Tool `self.control_dog_legs`: hỗ trợ các lệnh cử chỉ AI (`stand`, `sit`, `crouch`, `wiggle`, `wave`, `relax`).
   - Đã kết nối với bộ máy hành vi `DogBehavior`: tự động ngồi sau một thời gian idle, tự động vẫy mông/chân khi nói chuyện hoặc khi có cảm xúc vui vẻ (`kHappy`).

3. **Cảm biến Khoảng cách ToF Laser & Cảm biến Góc nghiêng MPU6050:**
   - Viết driver dùng chuẩn mới **ESP-IDF v6** (`driver/i2c_master.h`):
     - `vl53l0x.h`: Tự động nhận diện cả **VL53L0X** (8-bit reg, ID `0xEE`) và **VL53L1X** (16-bit reg, ID `0xEACC`).
     - `mpu6050.h`: Đọc gia tốc 3 trục, tính toán góc Pitch, Roll, phát hiện trạng thái bị nhấc bổng / bế lên (`is_picked_up`).
   - Đã đăng ký MCP Tools cho AI:
     - `self.get_front_distance`: AI có thể tự hỏi khoảng cách vật cản phía trước.
     - `self.get_motion_status`: AI biết mình đang đứng yên hay bị nhấc bổng, bị lật nghiêng.
   - Kích hoạt tính năng **chạm tay gần (< 1.5 cm) tự động đánh thức / lắng nghe**: Khi đưa tay sát cảm biến khoảng cách, cún tự vẫy đuôi, mắt vui vẻ và chuyển sang trạng thái nghe mà không cần nhấn nút BOOT hay gọi từ khóa.

---

## II. Kế Hoạch & Nhiệm Vụ Ngày Mai

### 1. Hoàn thành phần I2C (MPU6050 & VL53L0X / VL53L1X)
- [ ] **Kiểm tra phần cứng & đi dây:**
  - Kiểm tra lại kết nối dây GND chung giữa ESP32 và các module.
  - Test cắm lẻ từng module (MPU6050 riêng, ToF riêng) để loại trừ hiện tượng 1 module làm chập bus kéo I2C timeout.
  - Xác nhận đúng thứ tự chân SDA = GPIO8, SCL = GPIO9.
- [ ] **Xác nhận boot log:**
  - Cả 2 module phản hồi ACK ở địa chỉ `0x29` và `0x68`.
  - Kiểm tra thực tế tính năng đưa tay sát < 1.5 cm kích hoạt chế độ lắng nghe.

### 2. Tối ưu chuyển động 4 chân (Leg Servos)
- [ ] Căn chỉnh góc trim (`trim_deg`) và góc min/max (`SERVO_MIN_DEG`, `SERVO_MAX_DEG`) thực tế trên khung robot để các tư thế đứng thẳng, ngồi, cúi nằm vững chãi và cân bằng nhất.
- [ ] Tinh chỉnh các chuyển động: bước đi ngắn (gait), lắc mông (`wiggle`), giơ chân chào (`wave`).
- [ ] Tự động ngắt xung PWM servo khi đã giữ tư thế để chống nóng và tiết kiệm pin/chống sụt áp (brownout).

### 3. Tùy biến Chatbot & Persona thành "Cún Xiaozhi" (Dog Xiaozhi)
- [ ] Cập nhật System Prompt / Prompt trên Web Console Xiaozhi:
  - Khẳng định danh tính: Robot cún thông minh, có 4 chân, có cảm biến mắt mũi.
  - Khi người dùng bảo "đứng lên", "ngồi xuống", "chào đi", AI chủ động gọi tool `self.control_dog_legs` thay vì trả lời "tôi không có chân".
  - Giọng điệu thân thiện, hoạt bát, thỉnh thoảng sủa gâu gâu hoặc biểu đạt cảm xúc dễ thương.
- [ ] Phản ứng theo trạng thái MPU6050: Khi bị bế lên hoặc bị lật, cún thể hiện mắt ngạc nhiên/chóng mặt và kêu AI phản hồi tương ứng.

### 4. Hoàn thiện & Đóng gói sản phẩm
- [ ] Kiểm tra độ ổn định nguồn (pin / boost DC-DC) khi cả 4 servo cùng cử động và loa phát âm thanh.
- [ ] Tối ưu hóa bộ nhớ, kiểm tra rò rỉ RAM và hoàn thiện mã nguồn dự án.
