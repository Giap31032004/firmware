#include "mpu.h"
#include "kernel.h" 
#include "uart.h"   

/* === ĐỊNH NGHĨA 8 VÙNG NHỚ (FULL MAP CHO STM32F407) === */
#define R_FLASH       0  // Code (Static) - 1MB (STM32F407VG có 1MB Flash)
#define R_RAM         1  // Background SRAM1+SRAM2 (Static) - 128KB
#define R_PERIPH      2  // Ngoại vi (Static) - 512MB
#define R_SYSTEM      3  // NVIC/SCB/Systick/mpu (Static) - 512MB
#define R_NULL_GUARD  4  // Chặn NULL (Static) - 32 Bytes
#define R_STACK       5  // Stack Task (Dynamic) - Tùy Task
#define R_HEAP        6  // Heap Task (Dynamic) - Tùy Task
#define R_CCMRAM      7  // Core Coupled Memory (Static) - 64KB (Đặc sản STM32F4)

// Hàm khởi tạo MPU với các vùng nhớ cơ bản
void mpu_init(void) {
    /* 1. Tắt MPU để cấu hình an toàn */
    MPU_CTRL = 0;
    __DSB();

    /* --- REGION 0: FLASH (Code Space) ---
     * Base: 0x08000000 (Khác biệt số 1: Flash STM32 bắt đầu ở 0x08000000)
     * Size: 1MB (2^20)
     */
    MPU_RNR  = R_FLASH;
    MPU_RBAR = 0x08000000;
    MPU_RASR = (0 << MPU_RASR_XN_Pos)    | 
               (6 << MPU_RASR_AP_Pos)    | // Priv: RO, User: RO
               (1 << MPU_RASR_C_Pos)     | 
               (19 << MPU_RASR_SIZE_Pos) | // 2^20 = 1MB
               (1 << MPU_RASR_ENABLE_Pos);

    /* --- REGION 1: SRAM BACKGROUND (RAM Nền 128KB) ---
     * Base: 0x20000000
     * Size: 128KB (2^17)
     */
    MPU_RNR  = R_RAM;
    MPU_RBAR = 0x20000000;
    MPU_RASR = (1 << MPU_RASR_XN_Pos)    | 
               (3 << MPU_RASR_AP_Pos)    | // Priv: RW, User: RW
               (1 << MPU_RASR_C_Pos)     | 
               (1 << MPU_RASR_S_Pos)     | 
               (16 << MPU_RASR_SIZE_Pos) | // 2^17 = 128KB
               (1 << MPU_RASR_ENABLE_Pos);

    /* --- REGION 2: PERIPHERALS (Ngoại vi) ---
     * Base: 0x40000000, Size: 512MB
     */
    MPU_RNR  = R_PERIPH;
    MPU_RBAR = 0x40000000;
    MPU_RASR = (1 << MPU_RASR_XN_Pos)    |
               (3 << MPU_RASR_AP_Pos)    | // Priv: RW, User: RW
               (1 << MPU_RASR_B_Pos)     | 
               (28 << MPU_RASR_SIZE_Pos) | // 2^29 = 512MB
               (1 << MPU_RASR_ENABLE_Pos);

    /* --- REGION 3: SYSTEM PPB (Bảo vệ Lõi) --- */
    MPU_RNR  = R_SYSTEM;
    MPU_RBAR = 0xE0000000;
    MPU_RASR = (1 << MPU_RASR_XN_Pos)    |
               (1 << MPU_RASR_AP_Pos)    | // Priv: RW, User: NO ACCESS
               (28 << MPU_RASR_SIZE_Pos) | 
               (1 << MPU_RASR_ENABLE_Pos);

    /* --- REGION 4: NULL POINTER GUARD (Bẫy lỗi) ---
     * Mặc dù Flash STM32 ở 0x08000000, vùng 0x00000000 vẫn được map ảo.
     * Cần chặn vùng này để bắt lỗi con trỏ NULL.
     */
    MPU_RNR  = R_NULL_GUARD;
    MPU_RBAR = 0x00000000;
    MPU_RASR = (1 << MPU_RASR_XN_Pos)    |
               (0 << MPU_RASR_AP_Pos)    | // NO ACCESS
               (4 << MPU_RASR_SIZE_Pos)  | // 2^5 = 32 Bytes
               (1 << MPU_RASR_ENABLE_Pos);

    /* --- REGION 7: CCM RAM (Đặc sản của STM32F407) ---
     * Base: 0x10000000
     * Size: 64KB (2^16)
     * Vùng RAM này nối thẳng vào lõi CPU, chạy cực nhanh, rất hợp để làm RTOS Stack.
     */
    MPU_RNR  = R_CCMRAM;
    MPU_RBAR = 0x10000000; 
    MPU_RASR = (1 << MPU_RASR_XN_Pos)    |
               (3 << MPU_RASR_AP_Pos)    | // Priv: RW, User: RW
               (1 << MPU_RASR_C_Pos)     | 
               (15 << MPU_RASR_SIZE_Pos) | // 2^16 = 64KB
               (1 << MPU_RASR_ENABLE_Pos);

    /* Kích hoạt MemManage Fault và MPU */
    SCB_SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
    MPU_CTRL = MPU_CTRL_ENABLE_Msk | MPU_CTRL_PRIVDEFENA_Msk;
    
    __DSB();
    __ISB();
}

/* Hàm mpu_config_for_task GIỮ NGUYÊN 100% NHƯ CŨ 
 * Vì thuật toán bảo vệ Stack/Heap là logic trừu tượng, không phụ thuộc phần cứng!
 */
void mpu_config_for_task(PCB_t *task) {
    /* Tắt MPU tạm thời để cấu hình */
    MPU_CTRL = 0;
    __DSB();

    /* --- REGION 5: TASK STACK --- */
    MPU_RNR = R_STACK;
    MPU_RBAR = ((uint32_t)task->stack_base) & ~0x1F;
    uint32_t s_size = mpu_calc_region_size(task->stack_size);
    MPU_RASR = (1 << MPU_RASR_XN_Pos)    | 
               (3 << MPU_RASR_AP_Pos)    | 
               (1 << MPU_RASR_C_Pos)     | 
               (1 << MPU_RASR_S_Pos)     |
               (s_size << MPU_RASR_SIZE_Pos) |
               (1 << MPU_RASR_ENABLE_Pos);

    /* --- REGION 6: TASK HEAP (Nếu có) --- */
    if (task->heap_base != 0 && task->heap_size > 0) {
        MPU_RNR = R_HEAP;
        MPU_RBAR = ((uint32_t)task->heap_base) & ~0x1F;
        uint32_t h_size = mpu_calc_region_size(task->heap_size);
        MPU_RASR = (1 << MPU_RASR_XN_Pos) | 
                   (3 << MPU_RASR_AP_Pos) | 
                   (1 << MPU_RASR_C_Pos)  | 
                   (1 << MPU_RASR_S_Pos)  |
                   (h_size << MPU_RASR_SIZE_Pos) |
                   (1 << MPU_RASR_ENABLE_Pos);
    } else {
        MPU_RNR = R_HEAP;
        MPU_RASR &= ~(1 << MPU_RASR_ENABLE_Pos); 
    }

    /* Bật lại MPU */
    MPU_CTRL = MPU_CTRL_ENABLE_Msk | MPU_CTRL_PRIVDEFENA_Msk;
    __DSB();
    __ISB();
}

/* ----------------------------------------------------------------------------
 * MemManage Fault Handler (Màn hình xanh báo lỗi)
 * ĐÃ XÓA DMA TABLE VÀ CẬP NHẬT ĐỊA CHỈ FLASH
 * ---------------------------------------------------------------------------- */
void MemManage_Handler(void) {
    MPU_CTRL = 0; 
    uart_print("\r\n\033[1;31m[CRITICAL] *** MEMORY PROTECTION VIOLATION ***\033[0m\r\n");
    
    uint32_t fault_addr = SCB_MMFAR;
    uint32_t mmfsr = SCB_CFSR & 0xFF;

    if (mmfsr & (1 << 7)) { // Kiểm tra bit MMARVALID
        uart_print("Violation Address: 0x");
        uart_print_hex32(fault_addr);
        uart_print("\r\n");

        if (fault_addr < 0x20) {
            uart_print("=> CAUSE: NULL Pointer Dereference (Accessing 0x0)\r\n");
        } else if (fault_addr >= 0xE0000000) {
            uart_print("=> CAUSE: Illegal Access to System Registers (NVIC/SCB)\r\n");
        } else if (fault_addr >= 0x08000000 && fault_addr < 0x08100000) {
            uart_print("=> CAUSE: User tried to WRITE to Flash Memory (Code Space)\r\n");
        } else {
            uart_print("=> CAUSE: Stack Overflow or Unauthorized RAM Access\r\n");
        }
    } else {
        uart_print("=> CAUSE: Instruction Fetch Violation or Stacking Error\r\n");
    }

    if (current_pcb) {
        uart_print("Killing Offending Task PID: ");
        uart_print_dec(current_pcb->pid);
        uart_print("\r\n");
        current_pcb->state = PROC_SUSPENDED; 
    }

    SCB_CFSR |= 0xFF;
    uart_print("Action: Task Suspended. Triggering Context Switch...\r\n");

    *(volatile uint32_t *)0xE000ED04 = (1 << 28); 
    while(1); 
}