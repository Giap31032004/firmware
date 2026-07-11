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

# File Linker Script
LDSCRIPT = platform/linker_for_stm32f407.ld

# Includes
INCLUDES = \
    -I./kernel/include \
    -I./arch \
    -I./config \
    -I./platform \
    -I./boards/include \
    -I./app \
    -I./drivers \
    -I./hal \
    -I./cmsis/core \
    -I./cmsis/device/stm32f4

# Compiler Flags
FPU_FLAGS = -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS  = \
    -mcpu=cortex-m4 \
    -mthumb \
    $(FPU_FLAGS) \
    -DSTM32F407xx \
    -O2 \
    -ffreestanding \
    -nostdlib \
    -g \
    -Wall \
    -std=c99 \
    $(INCLUDES)

DEPFLAGS = -MMD -MP

ifneq ($(MYOS_TEST_SCENARIO),)
CFLAGS += -DMYOS_TEST_SCENARIO=$(MYOS_TEST_SCENARIO)
endif
ifneq ($(MYOS_BENCH_USE_DWT),)
CFLAGS += -DMYOS_BENCH_USE_DWT=$(MYOS_BENCH_USE_DWT)
endif
ifneq ($(MYOS_DIAG_DEMO),)
CFLAGS += -DMYOS_DIAG_DEMO=$(MYOS_DIAG_DEMO)
endif
# Linker Flags
LDFLAGS = -T $(LDSCRIPT) -nostdlib -Wl,-Map=$(BUILD_DIR)/$(TARGET).map

# ====================================================================
# 2. AUTO-DISCOVERY (TÌM KIẾM FILE TỰ ĐỘNG)
# ====================================================================
# Hàm đệ quy tìm file
rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

# DANH SÁCH THƯ MỤC CHUẨN (KHÔNG ĐỂ ĐÈ NHAU)
SEARCH_DIRS = app drivers hal kernel arch platform boards

C_SRCS = $(call rwildcard, $(SEARCH_DIRS), *.c)
ASM_SRCS = $(call rwildcard, $(SEARCH_DIRS), *.s)

# ====================================================================
# 3. OBJECT MAPPING (ÁNH XẠ FILE OBJECT VÀO THƯ MỤC BUILD)
# ====================================================================
C_OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(C_SRCS))
ASM_OBJS = $(patsubst %.s, $(BUILD_DIR)/%.o, $(ASM_SRCS))

OBJS = $(C_OBJS) $(ASM_OBJS)
DEPS = $(C_OBJS:.o=.d)

-include $(DEPS)

# ====================================================================
# 4. BUILD RULES (QUY TẮC BIÊN DỊCH)
# ====================================================================
.PHONY: all clean run run-trace run-debug run-gdb gdb run-renode run-renode-capture debug-renode

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
	@$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

# Biên dịch file .s (Assembly) thành .o
$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	@echo "  AS        $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# ====================================================================
# 5. TIỆN ÍCH (QEMU & RENODE & DEBUG)
# ====================================================================

# QEMU Config
QEMU_MACHINE = netduinoplus2
QEMU_FLAGS = -M $(QEMU_MACHINE) -kernel $(BUILD_DIR)/$(TARGET).elf -serial mon:stdio -nographic
RENODE_DIR = ./scripts/tools/renode/renode_1.16.1
RENODE = $(RENODE_DIR)/renode

run: all
	@echo "  QEMU      Running $(TARGET).elf on $(QEMU_MACHINE)"
	@qemu-system-arm $(QEMU_FLAGS)

run-trace: all
	@mkdir -p logs/run-trace
	@echo "  QEMU      Trace Mode (logs/run-trace/trace_instr.log)"
	@qemu-system-arm $(QEMU_FLAGS) -d in_asm -D logs/run-trace/trace_instr.log

run-debug: all
	@mkdir -p logs/run-debug
	@echo "  QEMU      CPU/IRQ Debug Mode (logs/run-debug/trace_cpu_irq.log)"
	@qemu-system-arm $(QEMU_FLAGS) -d cpu,int -D logs/run-debug/trace_cpu_irq.log

run-gdb: all
	@echo "  QEMU      Waiting for GDB connection on port 1234..."
	@qemu-system-arm $(QEMU_FLAGS) -S -gdb tcp::1234

gdb:
	gdb-multiarch $(BUILD_DIR)/$(TARGET).elf -ex "target remote localhost:1234"

run-renode:
	$(RENODE) --console --disable-xwt RenodeOfMe/stm32.resc

run-renode-capture:
	@mkdir -p logs/run-renode-capture
	bash scripts/run-renode-capture.sh --log-dir logs/run-renode-capture

debug-renode:
	gdb-multiarch $(BUILD_DIR)/$(TARGET).elf -ex "target remote :3333"

clean:
	@echo "  CLEAN     $(BUILD_DIR) and log files"
	@rm -rf $(BUILD_DIR) *.log logs/run-trace/*.log logs/run-debug/*.log logs/run-renode-capture/*.log
