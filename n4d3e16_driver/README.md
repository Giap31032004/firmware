# Driver N4D3E16 cho AMR — C++17

Phiên bản đầu chạy trên Jetson/Linux. Đây là thư viện driver và logic cảnh báo
nhỏ để tích hợp vào phần mềm robot; không có giao diện điều khiển người dùng.

## Luồng hoạt động

```mermaid
flowchart LR
    S[Cảm biến có tín hiệu số phù hợp] -->|Input điện| IO[N4D3E16]
    IO -->|Input qua RS485| J[Jetson: Driver và IoController]
    J -->|Lệnh output qua RS485| IO
    IO --> T[Thiết bị tải phù hợp]
```

Jetson/Linux là bộ điều khiển trong phiên bản hiện tại. Input và output thuộc
cùng module N4D3E16. Không có nguồn dữ liệu từ MCU, navigation hoặc phần mềm
nhận diện trong phiên bản này. Driver không phụ thuộc loại cảm biến hay tên tải.
Module cung cấp 16 input số, không đo trực tiếp giá trị nhiệt độ/analog.
Output ULN2803 là NPN kéo xuống mass, không phải tiếp điểm relay;
tải cần phù hợp thông số điện của module.

## Các phần mã nguồn

| Phần | Chức năng |
|---|---|
| `SerialPort` | Linux termios, raw 8N1, poll, đọc/ghi có timeout |
| `modbus_rtu` | CRC16, request FC03/06/10, ghép và kiểm tra response |
| `Driver` | API module, snapshot input, reporting, ACK và mã lỗi |
| `IoController` | Ánh xạ input số sang output theo các quy tắc |
| `src/main.cpp` / `amr_io` | Vòng lặp IO với mapping do người dùng cấu hình |

Không phụ thuộc ROS 2. Tất cả phương thức driver/controller chạy trên một luồng
sở hữu IO. Thư viện không có worker thread hoặc hàng đợi nội bộ.
Callback input chỉ cập nhật controller; `apply()` chạy sau khi giao dịch hoàn tất.
Khi idle, application gọi `process_once()` để nhận reporting.

## Quy tắc ánh xạ

Mỗi `IoRule` gồm `input_mask` (IN1–IN16) và danh sách output (CH1–CH16).
Bất kỳ input được chọn active thì quy tắc bật các output của nó.
Các quy tắc dùng chung output được tổng hợp OR: chỉ tắt khi mọi quy tắc liên quan
đều inactive. Output không được mapping sẽ không bị controller thay đổi.

```cpp
IoController controller({{0x0005, {1, 2}}, {0x8000, {2, 16}}},
    [&](uint8_t channel, bool active) {
        return active ? io.open(channel) : io.close(channel);
    });
io.set_input_callback([&](const InputState& state) {
    controller.update_inputs(state.active_mask);
});
```

Ví dụ: IN1 hoặc IN3 điều khiển CH1/CH2; IN16 điều khiển CH2/CH16.
CH2 còn bật khi bất kỳ nhóm nào còn active. Đây là cấu hình minh họa,
không mặc định dây nối thực tế. Mask 0, danh sách rỗng, kênh sai hoặc kênh
trùng trong cùng quy tắc bị từ chối; dùng chung kênh giữa các quy tắc được phép.

Trước mẫu input đầu tiên không gửi output. Sau mẫu đầu tiên, controller áp dụng
ON/OFF cho các output đã chọn. Chỉ lưu xác nhận sau ACK; trạng thái ổn định
không gửi lại. Nếu input đổi trong lúc chờ ACK, dừng lượt apply hiện tại và
áp dụng trạng thái mới ở lượt tiếp theo. Output ghi lần lượt, không đồng thời.
Controller phải là bên duy nhất điều khiển các output đã mapping.

`invalidate_output_confirmation()` xóa cache ACK sau thao tác output bên ngoài.
`reset()` xóa cả cache và input đã biết: gọi khi mất kết nối/trước reconnect,
cấu hình lại driver và chờ input mới trước `apply()`. Chưa tự reconnect,
chưa chốt chính sách khi input quá cũ hoặc module reset.
Không tự `close_all()` khi kết nối hoặc thoát chương trình.

## Build và test trên Ubuntu 24.04 / Jetson

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Cần compiler C++17 và CMake. Linux PTY test kiểm tra `termios`, byte bị chia nhỏ,
input report xen giữa response và mất kết nối. Không cần module thật để chạy test.

## Chạy chương trình với module thật

**Các tham số phải khớp dây nối thực tế.** Cú pháp mới:

```bash
./build/amr_io /dev/ttyUSB0 0 0x0005:1,2 0x8000:2,16
```

Thứ tự: `DEVICE REPORT_SECONDS INPUT_MASK:CH[,CH...] ...`.
Cho phép nhiều quy tắc; mask hỗ trợ 0x0001–0xFFFF. `0` tắt reporting,
ví dụ polling 100 ms; `1–255` là chu kỳ reporting theo giây.
Ví dụ dùng slave ID 1, baud 9600, timeout 250 ms, idle wait 20 ms trong mã nguồn.
Thư viện cho phép cấu hình slave 1–63 và baud/timeout qua `DriverConfig`.
Timing chỉ để chạy thử, chưa phải cam kết thời gian phản ứng.
CLI cũ với SENSOR_MASK/BUZZER_CH/LED_CH đã được thay thế.

`termios` không tự điều khiển DE/RE. Đường RS485 cần tự đổi chiều hoặc được cấu
hình qua khả năng RS485 của kernel/adapter. Code này chưa cấu hình `TIOCSRS485`.
Adapter phải tắt local echo; echo của FC06 giống hệt ACK và không phân biệt được
bằng payload. Quyền truy cập thiết bị serial do hệ thống Ubuntu quản lý.

## API output

| Hàm | Ý nghĩa / mã module |
|---|---|
| `open(ch)` | Bật: FC06, register ch, `0x0100` |
| `close(ch)` | Tắt: `0x0200` |
| `toggle(ch)` | Đảo trạng thái: `0x0300` |
| `interlock(ch)` | Native interlock: `0x0400` |
| `momentary(ch)` | Native momentary: `0x0500` |
| `delay(ch, seconds)` | Bật ngay, tự tắt sau 1–255 giây: `0x0600 | seconds` |
| `open_all()` | FC06, register 0, `0x0700` |
| `close_all()` | FC06, register 0, `0x0800` |
| `write_outputs(bitmap)` | FC06, register `0x0070`; thay đổi cả 16 kênh |
| `write_channels(commands)` | FC10; các kênh liên tiếp, native command values |
| `read_outputs(mask)` | FC03, register 1, quantity 16 |
| `read_output(ch, active)` | Đọc 16 output rồi lấy bit kênh yêu cầu |

Kênh 1–16, bit 0 = CH1/IN1. NPN điện mức thấp khi active, nhưng **protocol bit 1 = active**.
`momentary` và nhóm liên động cần đo trên firmware thật: command PDF ghi momentary
1 giây, manual PDF ghi 0,5 giây; quan hệ interlock toàn bộ 16 kênh chưa rõ.
Không có `blink`, `stop_blink`, GUI hoặc chính sách ưu tiên kiểu cảnh báo.

## Input và reporting

`configure_input_reporting(seconds)` gửi lần lượt:

1. `0x00FD = 0`: tắt reporting trong lúc cấu hình.
2. `0x00F9 = 0`, `0x00FA = 0`: tắt remote IO board-to-board.
3. `0x00FC = 1`: chọn input bitmap `0x00C0`.
4. `0x00FD = seconds`: 0 là polling; 1–255 là chu kỳ giây.

Phải cấu hình thành công trước `read_inputs()`/`read_outputs()` và vận hành.
Nếu cấu hình thất bại, module có thể đã nhận một phần cấu hình; không báo sẵn sàng.
`read_inputs()` đọc `0x00C0`. `get_inputs()` lấy snapshot đã nhận, không gửi frame.
`input_is_fresh(max_age)` cho application kiểm tra tuổi dữ liệu với ngưỡng tự chọn.
Input đầu tiên có `first_sample=true`, `changed_mask=0`; mẫu tiếp theo dùng XOR
để xác định các bit thay đổi. Callback vẫn nhận mọi mẫu để cập nhật timestamp.

**Giả định cần kiểm chứng:** unsolicited report ở chế độ bitmap có dạng
`[slave, 03, 02, mask_hi, mask_lo, crc_lo, crc_hi]`. Tài liệu không cung cấp ví dụ
wire format của unsolicited report đủ rõ để xác nhận giả định này. Đọc chủ động
theo FC03 là đường kiểm thử trước nếu firmware báo khác.

FC03 response không chứa register address. Vì vậy driver đọc **16 output register**
(32 data bytes), thay vì output bitmap (2 data bytes) để phân biệt với input report.
Không thêm các lệnh đọc một register khác vào cùng đường nhận này nếu chưa bổ
sung cơ chế phân loại. Khi đang đọc input, một report input mới cũng đủ cung cấp
snapshot; không tuyên bố nó nhất thiết là response của request vừa gửi.

Frame parser xử lý byte chia nhỏ và nhiều frame trong một lần đọc. Automatic
reporting là truyền tự phát trên bus half-duplex: phần mềm có thể phân loại frame
nhận được nhưng **không bảo đảm tránh va chạm vật lý**. Cần đo lúc reporting chạy
đồng thời với output command; nếu không ổn định, dùng polling với reporting tắt.

## Kết quả và lỗi

- Lệnh trả về `Result`: OK là ACK hợp lệ từ module, không chứng minh tải vật lý hoạt động.
- Timeout sau gửi hoặc lỗi có thể đã gửi một phần: `execution_unknown=true`.
- Timeout chờ bus idle trước gửi: chưa gửi lệnh, `execution_unknown=false`.
- Không tự retry các native command; toggle/delay/momentary có thể đổi hành vi khi gửi lại.
- `IoController` nhớ trạng thái ON/OFF sau ACK; trạng thái ổn định không gửi lặp.
  Nếu ACK lỗi, trạng thái đó không được ghi nhận thành công. Caller quyết định thử lại.
- Các kênh output được ghi lần lượt, chưa có cam kết chuyển đồng thời.
- Controller cần là bên duy nhất điều khiển các kênh đã mapping. Nếu dùng API khác
  thay đổi các kênh này, gọi `invalidate_output_confirmation()` để tính lại.
- Readback không diễn ra định kỳ: ACK cache chưa phát hiện module tự reset.
- Mất serial làm snapshot không còn valid; mẫu cũ có thể vẫn giữ bitmap để chẩn đoán.
- Chưa định nghĩa tự bật/tắt output khi mất input. Không
  giả định im lặng nghĩa là không có vật cản. Module có thể giữ output khi mất bus.
- Chưa tự reconnect. Sau reconnect cần cấu hình lại, gọi `IoController::reset()`
  và lấy input mới trước khi cho phép tự tắt cảnh báo dựa vào trạng thái cảm biến.

## Phạm vi kiểm chứng

Bản hiện tại được build bằng GCC 13.3.0 và chạy thành công 2/2 nhóm CTest trong Ubuntu 24.04
qua WSL, kiến trúc x86_64. Build đặt ngoài thư mục nguồn deliverable.

Mã dựa trên hai PDF N4D3E16 người dùng cung cấp: command và manual.
Test đối chiếu frame mẫu CRC từ PDF, mapping input/output và OR output dùng chung, input/ACK xen kẽ, sai CRC,
timeout, lỗi ghi một phần, exception và transport Linux qua pseudo-terminal.
Chưa kiểm thử trên Jetson ARM64 hay module N4D3E16 thật; chưa tích hợp vào ứng dụng AMR thực tế. Không suy ra thời gian đáp ứng hoặc bảo đảm an toàn chuyển
động từ các test phần mềm. Các quyết định tốc độ và hành vi khi lỗi sẽ chốt sau.
