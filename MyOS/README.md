# MyOS

MyOS là một hệ điều hành thời gian thực nhỏ chạy trên STM32F407/Cortex-M4. Dự án được xây dựng nhằm minh họa các thành phần cơ bản của một RTOS như quản lý task, lập lịch, đồng bộ, IPC, heap, trace, thống kê runtime và mô phỏng bằng Renode.

## Mục Tiêu

- Xây dựng kernel RTOS có khả năng tạo, quản lý và lập lịch task.
- Hỗ trợ preemptive scheduling, delay theo tick và context switch trên Cortex-M4.
- Cung cấp semaphore, mutex, message queue, heap động và software timer.
- Có shell UART để quan sát và điều khiển hệ thống khi chạy.
- Hỗ trợ mô phỏng STM32F407 bằng Renode.

## Cấu Trúc Thư Mục

```text
app/          Ứng dụng demo, shell, sensor, monitor, test RTOS
arch/         Mã phụ thuộc Cortex-M: context switch, ISR, NVIC, SysTick, MPU
boards/       BSP và cấu hình board
cmsis/        Header CMSIS cho ARM Cortex-M và STM32F4
config/       Cấu hình kernel và phần cứng
drivers/      Driver tầng thấp cho GPIO, UART
hal/          API phần cứng mức cao cho app
kernel/       Lõi RTOS: task, scheduler, sync, IPC, heap, timer, trace
platform/     Startup, linker script, system init STM32
RenodeOfMe/   Cấu hình mô phỏng Renode
scripts/      Script chạy Renode, capture log và test RTOS
```

## Yêu Cầu Môi Trường

Cần cài đặt các công cụ sau trong WSL/Linux:

```bash
arm-none-eabi-gcc
make
```

Nếu muốn chạy mô phỏng, cần có Renode. Dự án đã kèm Renode portable trong:

```text
scripts/tools/renode/
```

## Build

Từ thư mục gốc của project:

```bash
make clean
make
```

Kết quả build nằm trong:

```text
build/myos.elf
build/myos.bin
build/myos.map
```

## Chạy Bằng Renode

Sau khi build thành công:

```bash
make run-renode
```

Target này sẽ chạy script:

```text
RenodeOfMe/stm32.resc
```

Script Renode sẽ nạp file ELF và mở console UART để quan sát MyOS.

## Các Lệnh Shell Demo

Khi MyOS chạy, shell UART hỗ trợ các lệnh:

```text
help             Hiển thị danh sách lệnh
ps               Liệt kê task
stats            In thống kê CPU theo task
trace            In các sự kiện trace gần đây
traceclear       Xóa trace buffer
monitor on       Bật monitor định kỳ
monitor off      Tắt monitor định kỳ
monitor once     In thông tin hệ thống một lần
thermal          In trạng thái nhiệt độ demo
heap             In thông tin heap
queue            In thông tin message queue
power            In trạng thái tickless idle
demo             Mô tả luồng demo
demo features    In trạng thái các tính năng MyOS
stacks           Kiểm tra stack canary
kill <id>        Kết thúc một task
stop <id>        Tạm dừng một task
start <id>       Chạy lại một task
reboot           Khởi động lại hệ thống
```

## Luồng Demo Chính

Demo mặc định gồm các task:

```text
task_sensor
    Tạo nhiệt độ ảo, lọc trung bình và gửi dữ liệu qua message queue.

task_controller
    Nhận nhiệt độ, phân loại NORMAL/WARN/CRITICAL và điều khiển fan ảo.

task_shell
    Nhận lệnh từ UART.

task_runtime_monitor
    In thông tin runtime, heap, queue và task.

task_gpio_blink
    Demo GPIO và delay task.
```

## Tính Năng RTOS

- Tạo task với stack riêng.
- Preemptive scheduler.
- Round-robin cho các task cùng priority.
- SysTick và PendSV context switch.
- Semaphore và mutex.
- Priority inheritance cho mutex.
- Message queue IPC.
- Dynamic heap.
- Software timer.
- Runtime statistics.
- Trace event buffer.
- Stack canary check.
- MPU support.
- Tickless idle.

## Kiểm Thử RTOS

Dự án có bộ test trong:

```text
app/rtos_tests.c
```

Có thể build theo từng scenario bằng biến:

```bash
make clean
make MYOS_TEST_SCENARIO=<id>
```

Một số scenario kiểm thử:

```text
delay timeout
semaphore timeout
suspend/resume task
kill task
round-robin scheduling
mutex priority inheritance
heap fragmentation
stack overflow check
queue timeout
ISR semaphore
ISR queue
software timer
API latency
context switch
timer jitter
CPU load
```

## File Quan Trọng

```text
Makefile                         Quy tắc build/chạy
platform/startup.s               Vector table và Reset_Handler
platform/linker_for_stm32f407.ld Linker script chia Flash/RAM
platform/system_stm32f4xx.c      SystemInit và SystemCoreClock
RenodeOfMe/stm32.resc            Script chạy mô phỏng Renode
```


