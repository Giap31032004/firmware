# ====================================================================
# 1. PROJECT CONFIGURATION (CẤU HÌNH DỰ ÁN)
# ====================================================================
TARGET = myos
BUILD_DIR = build
BOARD = stm32f407

# Toolchain
PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy

# File Linker Script - Trỏ đúng vào file bạn đang có trong bsp/
LDSCRIPT = bsp/linker_for_stm32f407.ld

# Compiler Flags
FPU_FLAGS = -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS  = -mcpu=cortex-m4 -mthumb $(FPU_FLAGS) -O2 -ffreestanding -nostdlib -g -Wall -std=c99

# Linker Flags
LDFLAGS = -T $(LDSCRIPT) -nostdlib -Wl,-Map=$(BUILD_DIR)/$(TARGET).map

# Include Paths - SỬA LẠI CHỖ NÀY CHO ĐÚNG THỰC TẾ
INCLUDES = \
    -I./include \
    -I./bsp/include \
    -I./app \
    -I./kernel

CFLAGS += $(INCLUDES)

# ====================================================================
# 2. AUTO-DISCOVERY (TÌM KIẾM FILE TỰ ĐỘNG)
# ====================================================================
# Hàm đệ quy cực xịn: Tự động chui vào mọi thư mục con để mò file .c và .s
rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

SEARCH_DIRS = app arch bsp drivers hal kernel

C_SRCS = $(call rwildcard, $(SEARCH_DIRS), *.c)
ASM_SRCS = $(call rwildcard, $(SEARCH_DIRS), *.s)

# ====================================================================
# 3. OBJECT MAPPING (ÁNH XẠ FILE OBJECT VÀO THƯ MỤC BUILD)
# ====================================================================
C_OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(C_SRCS))
ASM_OBJS = $(patsubst %.s, $(BUILD_DIR)/%.o, $(ASM_SRCS))

OBJS = $(C_OBJS) $(ASM_OBJS)

# ====================================================================
# 4. BUILD RULES (QUY TẮC BIÊN DỊCH)
# ====================================================================
.PHONY: all clean run run-trace run-debug run-gdb gdb run-renode

all: $(BUILD_DIR)/$(TARGET).bin

# Tạo file .bin từ file .elf (Nạp trực tiếp xuống mạch thật)
$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	@echo "  OBJCOPY   $@"
	@$(OBJCOPY) -O binary $< $@
	@echo "  BUILD SUCCESS!"

# Link các file .o thành file .elf (Gói ghém thành file thực thi)
$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	@echo "  LD        $@"
	@$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

# Biên dịch file .c thành .o
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC        $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Biên dịch file .s (Assembly) thành .o
$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	@echo "  AS        $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# ====================================================================
# 5. TIỆN ÍCH (QEMU & RENODE & DEBUG)
# ====================================================================

# QEMU: Đã đổi máy ảo từ lm3s6965evb sang netduinoplus2 (Cortex-M4F)
QEMU_MACHINE = netduinoplus2
QEMU_FLAGS = -M $(QEMU_MACHINE) -kernel $(BUILD_DIR)/$(TARGET).elf -serial mon:stdio -nographic

run: all
	@echo "  QEMU      Running $(TARGET).elf on $(QEMU_MACHINE)"
	@qemu-system-arm $(QEMU_FLAGS)

run-trace: all
	@echo "  QEMU      Trace Mode (trace_instr.log)"
	@qemu-system-arm $(QEMU_FLAGS) -d in_asm -D trace_instr.log

run-debug: all
	@echo "  QEMU      CPU/IRQ Debug Mode (trace_cpu_irq.log)"
	@qemu-system-arm $(QEMU_FLAGS) -d cpu,int -D trace_cpu_irq.log

run-gdb: all
	@echo "  QEMU      Waiting for GDB connection on port 1234..."
	@qemu-system-arm $(QEMU_FLAGS) -S -gdb tcp::1234

gdb:
	$(PREFIX)gdb $(BUILD_DIR)/$(TARGET).elf -ex "target remote localhost:1234"

run-renode:
	./renode_1.15.3_portable/renode --console --disable-xwt stm32.resc

clean:
	@echo "  CLEAN     $(BUILD_DIR) and log files"
	@rm -rf $(BUILD_DIR) *.log