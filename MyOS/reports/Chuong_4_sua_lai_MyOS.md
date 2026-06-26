# CHƯƠNG 4. TRIỂN KHAI, KIỂM THỬ VÀ ĐÁNH GIÁ HỆ THỐNG

Sau khi phân tích yêu cầu và thiết kế kiến trúc MyOS ở Chương 3, chương này trình bày quá trình hiện thực hệ thống thành firmware có thể biên dịch và chạy trong môi trường mô phỏng. Nội dung chương tập trung vào bốn nhóm vấn đề: môi trường phát triển, tổ chức mã nguồn, luồng triển khai vận hành, và kết quả kiểm thử thực tế thông qua log UART/Renode.

Mục tiêu của chương không chỉ là liệt kê các module đã cài đặt, mà còn chứng minh rằng các thiết kế ở Chương 3 đã được hiện thực thành mã nguồn có khả năng vận hành. Vì vậy, bên cạnh mô tả cấu trúc dự án, chương này bổ sung các workflow build/chạy hệ thống, bảng liên hệ giữa module thiết kế và file mã nguồn, bảng kịch bản kiểm thử, log kết quả và phân tích ý nghĩa từng nhóm log.

## 4.1. Môi trường và công cụ phát triển

MyOS được triển khai cho nền tảng vi điều khiển ARM Cortex-M4F, hướng đến dòng STM32F407. Hệ thống được xây dựng theo mô hình firmware bare-metal, chạy trực tiếp trên phần cứng hoặc môi trường mô phỏng, không phụ thuộc vào hệ điều hành bên dưới. Do đó, môi trường phát triển cần hỗ trợ lập trình mức thấp, biên dịch chéo, liên kết bộ nhớ, chạy mô phỏng và quan sát kết quả qua UART.

Ngôn ngữ chính được sử dụng là C theo chuẩn C99. Các thành phần lõi như quản lý task, scheduler, tick, software timer, semaphore, mutex, message queue, heap, trace và runtime statistics được viết bằng C. Một số phần phụ thuộc trực tiếp vào kiến trúc Cortex-M4F, đặc biệt là khởi động task đầu tiên và chuyển ngữ cảnh, được viết bằng ARM Assembly để thao tác với PSP, SVC, PendSV và các thanh ghi CPU.

**Bảng 4.1. Công cụ và vai trò trong quá trình phát triển MyOS**

| Công cụ / thành phần | Vai trò |
|---|---|
| C99 | Triển khai phần lớn logic kernel, driver và ứng dụng kiểm thử. |
| ARM Assembly | Hiện thực các đoạn phụ thuộc kiến trúc như SVC, PendSV và chuyển ngữ cảnh. |
| GNU Arm Embedded Toolchain | Biên dịch và liên kết firmware cho Cortex-M4F. |
| Linker script | Quy định bố trí FLASH, RAM, stack, heap, vector table và các section. |
| Makefile | Tự động hóa quá trình build, tạo ELF/BIN và cấu hình kịch bản kiểm thử. |
| QEMU | Chạy nhanh firmware ARM và hỗ trợ kiểm tra ở mức mô phỏng CPU. |
| Renode | Mô phỏng nền tảng STM32F407, quan sát log UART và hỗ trợ kiểm thử lặp lại. |
| UART log | Ghi nhận kết quả boot, trạng thái kernel và kết quả từng kịch bản kiểm thử. |

## 4.2. Tổ chức mã nguồn và quy trình build

Mã nguồn MyOS được tổ chức theo hướng phân lớp để tách phần ứng dụng, kernel và các thành phần phụ thuộc phần cứng. Cách tổ chức này giúp hệ thống dễ bảo trì, thuận tiện cho kiểm thử từng nhóm chức năng và giảm sự phụ thuộc của kernel vào chi tiết phần cứng cụ thể.

**Bảng 4.2. Tổ chức thư mục mã nguồn MyOS**

| Thư mục | Vai trò |
|---|---|
| `app/` | Chứa chương trình chính, ứng dụng minh họa, UART shell, monitor task và các kịch bản kiểm thử RTOS. |
| `kernel/include/` | Chứa các header định nghĩa API, kiểu dữ liệu và cấu trúc quản lý của kernel. |
| `kernel/source/` | Chứa mã nguồn triển khai các thành phần lõi như task, scheduler, sync, IPC, heap, timer, trace. |
| `arch/` | Chứa phần phụ thuộc Cortex-M4F như PendSV, SVC, SysTick, NVIC, MPU và port layer. |
| `platform/` | Chứa startup, linker script, memory map và phần khởi tạo nền tảng. |
| `boards/` | Chứa mã khởi tạo board, clock, SCB và BSP. |
| `drivers/` | Chứa driver ngoại vi mức thấp như UART, GPIO và giao diện IPC dùng trong ứng dụng. |
| `hal/` | Cung cấp lớp trừu tượng hóa ngoại vi mức cao hơn cho ứng dụng. |
| `config/` | Chứa cấu hình kernel và phần cứng như số task, priority, stack, trace, tickless idle. |
| `RenodeOfMe/` | Chứa script và mô tả nền tảng dùng để chạy mô phỏng Renode. |
| `logs/` | Lưu log kết quả chạy mô phỏng và kiểm thử. |

Quy trình build của MyOS được tự động hóa bằng Makefile. Khi build, hệ thống tự động tìm các file C và Assembly trong các thư mục nguồn, biên dịch thành object file, sau đó liên kết bằng linker script để tạo file ELF. Từ file ELF, công cụ `objcopy` tạo file BIN dùng cho mô phỏng hoặc nạp firmware.

**Hình 4.1. Quy trình build firmware MyOS**

```text
Mã nguồn C/Assembly
        ↓
Biên dịch bằng arm-none-eabi-gcc
        ↓
Tạo object file
        ↓
Liên kết bằng linker script
        ↓
Tạo myos.elf
        ↓
objcopy
        ↓
Tạo myos.bin
```

Makefile cũng hỗ trợ lựa chọn kịch bản kiểm thử thông qua macro cấu hình `MYOS_TEST_SCENARIO`. Nhờ đó, cùng một cây mã nguồn có thể chạy ứng dụng minh họa hoặc từng nhóm test riêng cho task, scheduler, timer, IPC, đồng bộ hóa và bộ nhớ.

## 4.3. Luồng khởi động và vận hành hệ thống

Luồng khởi động của MyOS bắt đầu từ startup code và hàm `main()`. Sau khi board được khởi tạo, kernel thiết lập các cấu trúc quản lý nội bộ, ứng dụng tạo các task cần thiết, sau đó scheduler được khởi động để chuyển CPU sang task đầu tiên.

**Hình 4.2. Luồng khởi động và vận hành của MyOS**

```text
Reset / Startup
        ↓
Thiết lập vector table, stack, dữ liệu khởi tạo
        ↓
main()
        ↓
board_init()
        ↓
kernel_init()
        ↓
service_init()
        ↓
app_init() hoặc rtos_test_init()
        ↓
os_start()
        ↓
SVC khởi động task đầu tiên
        ↓
SysTick / PendSV / Scheduler điều phối task
```

Trong luồng trên, `kernel_init()` khởi tạo các thành phần lõi như task table, ready list, scheduler, tick, heap, software timer, trace và runtime statistics. Sau đó, `app_init()` hoặc `rtos_test_init()` tạo các task ứng dụng hoặc task kiểm thử. Khi gọi `os_start()`, scheduler chọn task READY có độ ưu tiên cao nhất, port layer cấu hình SysTick, đặt mức ưu tiên cho PendSV/SVC/SysTick và dùng SVC để đưa CPU vào task đầu tiên.

## 4.4. Triển khai các module chính

Các thiết kế ở Chương 3 được hiện thực thành các module mã nguồn tương ứng. Bảng 4.3 tóm tắt mối liên hệ giữa nhóm chức năng, file triển khai và vai trò của từng module.

**Bảng 4.3. Liên hệ giữa module thiết kế và mã nguồn triển khai**

| Module | File triển khai chính | Nội dung triển khai |
|---|---|---|
| Kernel core | `kernel/source/kernel.c`, `kernel/include/kernel.h` | Khởi tạo kernel, API delay, panic và các macro dùng chung. |
| Task Management | `kernel/source/task.c`, `kernel/include/task.h` | Tạo task tĩnh/động, quản lý trạng thái, stack, block/wakeup, suspend/resume, terminate và cleanup. |
| Scheduler | `kernel/source/scheduler.c`, `kernel/include/scheduler.h` | Ready list, priority bitmap, preemption, round-robin, time slicing và scheduler lock. |
| Context switch | `arch/context_switch.s`, `arch/port.c` | SVC khởi động task đầu tiên, PendSV lưu/khôi phục ngữ cảnh, PSP và exception return. |
| Tick và thời gian | `kernel/source/tick.c`, `arch/systick.c` | Kernel tick, delay, timeout và nguồn tick từ SysTick. |
| Software timer | `kernel/source/timer.c`, `kernel/include/timer.h` | Timer one-shot/periodic, danh sách timer và callback khi đến hạn. |
| Đồng bộ hóa | `kernel/source/sync.c`, `kernel/include/sync.h` | Semaphore, mutex, wait list, timeout và priority inheritance. |
| IPC | `kernel/source/ipc.c`, `drivers/ipc.h` | Message queue, buffer vòng, send/receive timeout và phối hợp task-task/task-ISR. |
| Heap | `kernel/source/heap.c`, `kernel/include/heap.h` | Cấp phát/giải phóng bộ nhớ động, split/merge block và thống kê heap. |
| MPU | `arch/mpu.c`, `kernel/include/mpu.h` | Cấu hình vùng nhớ tĩnh/động, cập nhật vùng MPU theo task. |
| Trace | `kernel/source/os_trace.c`, `kernel/include/os_trace.h` | Ghi vết sự kiện kernel như task switch, block, ready, semaphore, mutex, queue. |
| Runtime stats | `kernel/source/runtime_stats.c`, `kernel/include/runtime_stats.h` | Tích lũy thời gian CPU của từng task và in thống kê runtime. |
| UART shell/log | `drivers/uart.c`, `app/tasks/shell.c`, `kernel/source/os_log.c` | In log UART, nhận lệnh shell và quan sát trạng thái hệ thống. |

Việc chia module như trên giúp phần lõi kernel ít phụ thuộc vào chi tiết phần cứng. Các thao tác phụ thuộc Cortex-M4F được đặt trong `arch/`, trong khi logic RTOS độc lập hơn với phần cứng được đặt trong `kernel/`. Ứng dụng minh họa và kiểm thử được đặt trong `app/`, cho phép thay đổi kịch bản chạy mà không ảnh hưởng trực tiếp đến kernel.

## 4.5. Kịch bản kiểm thử hệ thống

Các kịch bản kiểm thử được xây dựng theo từng nhóm chức năng chính của kernel. Mỗi kịch bản tập trung vào một cơ chế cụ thể, sau đó kết quả được quan sát qua UART log trong môi trường mô phỏng. Cách kiểm thử này giúp đánh giá từng chức năng độc lập trước khi kết luận về hoạt động tổng thể của hệ thống.

**Bảng 4.4. Các kịch bản kiểm thử chức năng MyOS**

| Mã scenario | Nhóm kiểm thử | Nội dung kiểm tra | Kết quả mong đợi |
|---|---|---|---|
| 1 | Delay/timeout | Task delay trong số tick xác định | Task được đánh thức sau khoảng tick mong đợi. |
| 2 | Semaphore timeout | Task chờ semaphore có timeout | Hàm trả về `OS_TIMEOUT` khi không có tín hiệu. |
| 3 | Suspend/resume | Tạm dừng và tiếp tục task đang delay | Task bị suspend không được chạy, resume thì tiếp tục được đánh thức. |
| 4 | Kill task đang chờ | Hủy task đang block trên semaphore | Task bị hủy không được đánh thức sai sau khi signal. |
| 5 | Round-robin | Hai task cùng priority chia sẻ CPU | Hai task luân phiên chạy và số lần switch đạt kỳ vọng. |
| 6 | Mutex priority inheritance | Task ưu tiên cao chờ mutex do task thấp giữ | Owner được nâng priority tạm thời, hạn chế priority inversion. |
| 7 | Heap fragmentation | Cấp phát/giải phóng nhiều block | Heap biết gộp block trống và phản ánh phân mảnh. |
| 8 | Stack overflow | Làm hỏng stack canary | Kernel phát hiện lỗi stack. |
| 10 | Queue timeout | Gửi khi queue đầy, nhận khi queue rỗng | Send/receive trả về timeout đúng điều kiện. |
| 12 | ISR semaphore | ISR báo hiệu semaphore cho task | Task đang chờ được đánh thức đúng. |
| 13 | Binary semaphore | Kiểm tra giới hạn count của binary semaphore | Semaphore không vượt quá giá trị tối đa. |
| 14 | Counting semaphore | Kiểm tra count và timeout của counting semaphore | Count được giới hạn và thao tác chờ trả kết quả đúng. |
| 17 | Software timer | Timer one-shot gọi callback một lần | Callback chạy đúng một lần và test PASS. |

**Hình 4.3. Quy trình kiểm thử trong môi trường mô phỏng**

```text
Chọn MYOS_TEST_SCENARIO
        ↓
Biên dịch firmware
        ↓
Nạp firmware vào QEMU/Renode
        ↓
Chạy mô phỏng STM32F407
        ↓
Quan sát log UART
        ↓
So sánh kết quả thực tế với kết quả mong đợi
        ↓
Kết luận PASS/FAIL cho từng kịch bản
```

Các kịch bản kiểm thử được thiết kế để có thể chạy lặp lại. Khi thay đổi mã nguồn kernel, người phát triển có thể build lại firmware với scenario tương ứng và đối chiếu UART log để kiểm tra xem chức năng có còn hoạt động đúng hay không.

## 4.6. Kết quả kiểm thử và phân tích log

### 4.6.1. Kết quả khởi động trong Renode

Một lần chạy kiểm thử trong Renode tạo ra log như sau:

```text
20:58:42.0111 [INFO] System bus created.
20:58:43.6198 [INFO] sysbus: Loading block of 580 bytes length at 0x8000000.
20:58:43.6318 [INFO] sysbus: Loading block of 20132 bytes length at 0x8010000.
20:58:43.7300 [INFO] cpu: Setting initial values: PC = 0x8000189, SP = 0x20008000.
20:58:43.7326 [INFO] stm32f407_board: Machine started.
20:58:43.7937 [INFO] usart1: Booting MyOS Kernel...
20:58:43.7964 [INFO] usart1: Kernel Initialized. [OK]
```

**Bảng 4.5. Phân tích log khởi động MyOS trong Renode**

| Nhóm log | Ý nghĩa | Đánh giá |
|---|---|---|
| `System bus created` | Renode đã tạo bus hệ thống cho mô hình STM32F407. | Môi trường mô phỏng được thiết lập. |
| `Loading block ... at 0x8000000` | Firmware được nạp vào vùng Flash bắt đầu từ địa chỉ vector table. | File ELF/BIN được load đúng vùng nhớ. |
| `PC = 0x8000189, SP = 0x20008000` | CPU lấy PC và SP ban đầu từ vector table. | Luồng reset/startup bắt đầu đúng. |
| `Machine started` | Máy mô phỏng đã chuyển sang trạng thái chạy. | Mô phỏng bắt đầu thực thi firmware. |
| `Booting MyOS Kernel...` | Firmware đã chạy đến phần khởi động kernel và in log qua UART. | UART và luồng boot hoạt động. |
| `Kernel Initialized. [OK]` | Kernel khởi tạo thành công các cấu trúc quản lý chính. | Kernel sẵn sàng tạo task và chạy scheduler. |

Kết quả trên cho thấy firmware đã được nạp và thực thi trong mô hình STM32F407. CPU lấy giá trị PC/SP ban đầu từ vector table, máy mô phỏng bắt đầu chạy, UART xuất được log khởi động và kernel hoàn tất khởi tạo. Đây là điều kiện cần trước khi đánh giá các chức năng RTOS ở các kịch bản kiểm thử tiếp theo.

### 4.6.2. Kết quả kiểm thử software timer

Kịch bản software timer kiểm tra khả năng khởi tạo timer, đưa timer vào danh sách quản lý, xử lý timer theo tick hệ thống và gọi callback đúng thời điểm. Log thu được như sau:

```text
[RTOS_TEST] scenario=17
[RTOS_TEST] oneshot_count=1
[RTOS_TEST] software_timer PASS
Matched 'software_timer PASS': 1
```

Trong mã kiểm thử, scenario 17 tương ứng với `RTOS_TEST_SOFTWARE_TIMER`. Task kiểm thử khởi tạo một timer one-shot, bắt đầu timer với thời hạn 3 tick, sau đó delay 5 tick để chờ timer hết hạn. Callback của timer tăng biến `timer_oneshot_count`. Nếu giá trị cuối cùng bằng 1, kịch bản được xem là đạt.

```c
os_timer_init(&test_oneshot_timer, software_timer_oneshot_cb, NULL);
os_timer_start(&test_oneshot_timer, 3U);
task_delay(5);
finish_test(timer_oneshot_count == 1U, "software_timer");
```

**Bảng 4.6. Phân tích kết quả kiểm thử software timer**

| Dòng log / kết quả | Ý nghĩa | Đánh giá |
|---|---|---|
| `[RTOS_TEST] scenario=17` | Firmware đang chạy kịch bản kiểm thử software timer. | Đúng kịch bản. |
| `oneshot_count=1` | Callback của timer one-shot được gọi đúng một lần. | Đúng kỳ vọng. |
| `software_timer PASS` | Điều kiện kiểm thử trong firmware trả về đạt. | Test đạt. |
| `Matched 'software_timer PASS': 1` | Script thu log tìm thấy đúng chuỗi PASS. | Kết quả được ghi nhận tự động. |

Kết quả này chứng minh rằng software timer đã phối hợp đúng với kernel tick: timer được kích hoạt, được kiểm tra khi tick tăng, callback được gọi khi đến hạn và timer one-shot không tiếp tục gọi callback nhiều lần sau khi hoàn thành. Đây cũng cho thấy cơ chế delay của task kiểm thử và cơ chế xử lý timer có thể phối hợp với nhau trong cùng hệ thống thời gian.

## 4.7. Đánh giá kết quả triển khai

Qua quá trình triển khai và kiểm thử, MyOS đã hiện thực được các thành phần cốt lõi của một RTOS nhúng trên nền tảng ARM Cortex-M4F. Hệ thống có khả năng tạo và quản lý nhiều task, lập lịch theo độ ưu tiên, chuyển ngữ cảnh bằng PendSV, quản lý thời gian bằng SysTick, hỗ trợ delay/timeout, semaphore, mutex, message queue, software timer, heap và các cơ chế chẩn đoán cơ bản.

**Bảng 4.7. Tổng hợp đánh giá các nhóm chức năng**

| Nhóm chức năng | Kết quả triển khai | Nhận xét |
|---|---|---|
| Task Management | Đã hỗ trợ tạo task, delay, block/wakeup, suspend/resume, terminate và cleanup. | Đạt mức chức năng cơ bản của RTOS nhỏ. |
| Scheduler | Đã có priority scheduling, preemption, round-robin và time slicing. | Cần đo thêm scheduler latency trên phần cứng thật. |
| Context switch | Đã dùng SVC/PendSV/PSP để chuyển task. | Cần đo thêm thời gian chuyển ngữ cảnh. |
| Tick và timer | Đã có kernel tick, delay, timeout, software timer và tickless idle ở mức thiết kế/triển khai. | Software timer đã có log kiểm thử PASS. |
| Đồng bộ hóa | Đã có semaphore, mutex và priority inheritance. | Cần mở rộng thêm kiểm thử cạnh tranh phức tạp. |
| IPC | Đã có message queue với timeout. | Cần đánh giá thêm throughput và tình huống task/ISR tải cao. |
| Bộ nhớ | Đã có heap, stack tĩnh/động, canary và high-water mark. | Cần kiểm thử dài hạn để đánh giá phân mảnh. |
| MPU | Đã có cấu hình vùng nhớ theo task ở mức cơ bản. | Chưa có user mode/syscall hoàn chỉnh. |
| Chẩn đoán | Đã có UART log, trace và runtime statistics. | Cần chuẩn hóa log và bổ sung crash dump nếu mở rộng. |

Tuy các kết quả trên cho thấy MyOS đã hoạt động đúng ở mức chức năng trong môi trường mô phỏng, hệ thống vẫn chưa đủ cơ sở để xem là một RTOS sẵn sàng dùng trong sản phẩm thực tế. Các chỉ tiêu thời gian thực như độ trễ ngắt, thời gian chuyển ngữ cảnh, scheduler latency và jitter của task định kỳ chưa được đo định lượng trên phần cứng thật. Ngoài ra, MPU mới dừng ở mức nền tảng bảo vệ, task vẫn chạy ở privileged mode và chưa có lớp system call hoàn chỉnh.

## 4.8. Kết chương

Chương 4 đã trình bày quá trình triển khai, kiểm thử và đánh giá hệ thống MyOS trên nền tảng ARM Cortex-M4F. Nội dung chương liên kết thiết kế ở Chương 3 với mã nguồn thực tế thông qua tổ chức thư mục, module triển khai, quy trình build, luồng khởi động và các kịch bản kiểm thử.

Kết quả chạy mô phỏng Renode cho thấy firmware được nạp và thực thi đúng, kernel khởi tạo thành công và UART log ghi nhận được trạng thái hệ thống. Kịch bản kiểm thử software timer cho thấy timer one-shot gọi callback đúng một lần và trả về kết quả PASS. Các kịch bản kiểm thử khác được tổ chức theo cùng nguyên tắc: lựa chọn scenario, chạy mô phỏng, thu log UART và so sánh kết quả thực tế với kết quả mong đợi.

Nhìn chung, MyOS đã đạt được mục tiêu xây dựng một RTOS nhúng nhỏ gọn ở mức nghiên cứu, có khả năng vận hành các cơ chế cơ bản trong môi trường mô phỏng. Các kết quả này là cơ sở để tiếp tục kiểm thử trên phần cứng STM32F407 thật, đo đạc các chỉ tiêu thời gian thực và mở rộng các cơ chế bảo vệ, driver và chẩn đoán hệ thống trong các bước phát triển tiếp theo.
