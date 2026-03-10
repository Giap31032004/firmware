# ====================================================================
# 1. PROJECT CONFIGURATION
# ====================================================================
TARGET = myos
BUILD_DIR = build

# Toolchain (Sử dụng đường dẫn mặc định trong biến môi trường)
PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy

# [QUAN TRỌNG] File Linker Script
# Hiện tại trong thư mục bsp của bạn đang trống.
# Bạn cần đảm bảo có file linker (VD: bsp/linker.ld) để build thành công.
LDSCRIPT = bsp/linker.ld

# Compiler & Linker Flags
CFLAGS  = -mcpu=cortex-m3 -mthumb -O2 -ffreestanding -nostdlib -g -Wall -std=c99
# Thêm cờ tạo file .map để bạn dễ dàng debug xem hàm nào tốn bao nhiêu RAM/Flash
LDFLAGS = -T $(LDSCRIPT) -nostdlib -Wl,-Map=$(BUILD_DIR)/$(TARGET).map

# Include Paths (Chỉ ra nơi trình biên dịch đi tìm file .h)
INCLUDES = \
	-I./include \
	-I./config \
	-I./app \
	-I./kernel

CFLAGS += $(INCLUDES)

# ====================================================================
# 2. AUTO-DISCOVERY (TÌM KIẾM FILE TỰ ĐỘNG ĐA NỀN TẢNG)
# ====================================================================
# Hàm rwildcard: Kỹ thuật đệ quy thuần Make để tìm file trong các thư mục con
rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

# Chỉ định các thư mục gốc cần quét
SEARCH_DIRS = app arch bsp drivers hal kernel

# Thu thập toàn bộ file .c và .s
C_SRCS = $(call rwildcard, $(SEARCH_DIRS), *.c)
ASM_SRCS = $(call rwildcard, $(SEARCH_DIRS), *.s)

# ====================================================================
# 3. OBJECT MAPPING (ÁNH XẠ VÀO THƯ MỤC BUILD)
# ====================================================================
# Đổi đuôi file nguồn thành .o và gắn tiền tố "build/"
C_OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(C_SRCS))
ASM_OBJS = $(patsubst %.s, $(BUILD_DIR)/%.o, $(ASM_SRCS))

OBJS = $(C_OBJS) $(ASM_OBJS)

# ====================================================================
# 4. BUILD RULES
# ====================================================================
# Để tránh xung đột với các file có tên trùng với target
.PHONY: all clean run

# Mục tiêu mặc định
all: $(BUILD_DIR)/$(TARGET).bin

# Rule 1: Tạo file .bin từ file .elf
$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	@echo "------------------------------------------------"
	@echo "  OBJCOPY   $@"
	@$(OBJCOPY) -O binary $< $@
	@echo "  BUILD SUCCESS!"
	@echo "------------------------------------------------"

# Rule 2: Link các file object (.o) thành file .elf
$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	@echo "  LD        $@"
	@$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

# Rule 3: Biên dịch file .c thành .o
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC        $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Rule 4: Biên dịch file .s thành .o
$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	@echo "  AS        $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# ====================================================================
# 5. TIỆN ÍCH (RUN & DEBUG)
# ====================================================================
# Để tránh xung đột với các file có tên trùng với target
.PHONY: all clean run run-trace run-debug run-gdb gdb

# Chạy QEMU bình thường
run: all
	@echo "  QEMU      Running $(TARGET).elf"
	qemu-system-arm -M lm3s6965evb -kernel $(BUILD_DIR)/$(TARGET).elf -serial mon:stdio -nographic

# Chế độ theo dõi tập lệnh (Instruction Trace)
# Lưu toàn bộ các lệnh assembly mà CPU thực thi vào file trace_instr.log
run-trace: all
	@echo "  QEMU      Running in Trace Mode (saving to trace_instr.log)"
	qemu-system-arm -M lm3s6965evb -kernel $(BUILD_DIR)/$(TARGET).elf -serial mon:stdio -nographic \
	-d in_asm -D trace_instr.log

# Chế độ theo dõi Ngắt và Ngữ cảnh CPU (Rất quan trọng cho RTOS)
# Ghi lại sự thay đổi các thanh ghi và khi nào ngắt (SysTick, PendSV) xảy ra
run-debug: all
	@echo "  QEMU      Running in CPU/IRQ Debug Mode (saving to trace_cpu_irq.log)"
	qemu-system-arm -M lm3s6965evb -kernel $(BUILD_DIR)/$(TARGET).elf -serial mon:stdio -nographic \
	-d cpu,int -D trace_cpu_irq.log

# Chế độ chờ GDB Debugger gắn vào (Dừng hệ thống ở lệnh đầu tiên)
run-gdb: all
	@echo "  QEMU      Waiting for GDB connection on port 1234..."
	qemu-system-arm -M lm3s6965evb -kernel $(BUILD_DIR)/$(TARGET).elf -serial mon:stdio -nographic \
	-S -gdb tcp::1234

# Mở GDB và tự động kết nối với QEMU (Mở ở một Terminal khác)
gdb:
	$(PREFIX)gdb $(BUILD_DIR)/$(TARGET).elf -ex "target remote localhost:1234"

# Dọn dẹp thư mục build và các file log
clean:
	@echo "  CLEAN     $(BUILD_DIR) and log files"
	@rm -rf $(BUILD_DIR) *.log