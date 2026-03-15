#include "mpu.h"
#include "kernel.h" 
#include "uart.h"   

/* === ĐỊNH NGHĨA 8 VÙNG NHỚ (FULL MAP) === */
#define R_FLASH       0  // Code (Static) - 256KB
#define R_RAM         1  // Background RAM (Static) - 64KB
#define R_PERIPH      2  // Ngoại vi (Static) - 512MB
#define R_SYSTEM      3  // NVIC/SCB/Systick/mpu (Static) - 512MB
#define R_NULL_GUARD  4  // Chặn NULL (Static) - 32 Bytes
#define R_STACK       5  // Stack Task (Dynamic) - Tùy Task
#define R_HEAP        6  // Heap Task (Dynamic) - Tùy Task
#define R_DMA         7  // DMA Table (Static) - 1KB

/* * Liên kết đến bảng điều khiển DMA. 
 * Yêu cầu: Trong drivers/dma/dma.c biến này phải bỏ 'static' 
 * và được căn chỉnh 1024 bytes (__attribute__((aligned(1024))))
 */
extern uint32_t dma_table[]; 

// Hàm khởi tạo MPU với các vùng nhớ cơ bản
void mpu_init(void) {
    /* 1. Tắt MPU để cấu hình an toàn */
    MPU_CTRL = 0;
    __DSB();
    /* --- REGION 0: FLASH (Code Space) ---
     * Base: 0x00000000, Size: 256KB
     * Attr: Executable (XN=0), User Read-Only (AP=6), Cacheable
     * Mục đích: Cho phép chạy code, nhưng User không được ghi đè Flash.
     */
    MPU_RNR  = R_FLASH;
    MPU_RBAR = 0x00000000;
    MPU_RASR = (0 << MPU_RASR_XN_Pos)    | 
               (6 << MPU_RASR_AP_Pos)    | // Priv: RO, User: RO
               (1 << MPU_RASR_C_Pos)     | 
               (17 << MPU_RASR_SIZE_Pos) | // 2^18 = 256KB
               (1 << MPU_RASR_ENABLE_Pos);

    /* --- REGION 1: SRAM BACKGROUND (RAM Nền) ---
     * Base: 0x20000000, Size: 64KB
     * Attr: No-Exec (XN=1), User Read-Write (AP=3), Shareable
     * Mục đích: Chứa biến toàn cục, Queue, Mutex dùng chung.
     */
    MPU_RNR  = R_RAM;
    MPU_RBAR = 0x20000000;
    MPU_RASR = (1 << MPU_RASR_XN_Pos)    | 
               (3 << MPU_RASR_AP_Pos)    | // Priv: RW, User: RW
               (1 << MPU_RASR_C_Pos)     | 
               (1 << MPU_RASR_S_Pos)     | 
               (15 << MPU_RASR_SIZE_Pos) | // 2^16 = 64KB
               (1 << MPU_RASR_ENABLE_Pos);

    /* --- REGION 2: PERIPHERALS (Ngoại vi) ---
     * Base: 0x40000000, Size: 512MB
     * Attr: No-Exec, User Read-Write, Bufferable
     * Mục đích: Cho phép Task truy cập UART, GPIO, Timer...
     */
    MPU_RNR  = R_PERIPH;
    MPU_RBAR = 0x40000000;
    MPU_RASR = (1 << MPU_RASR_XN_Pos)    |
               (3 << MPU_RASR_AP_Pos)    | // Priv: RW, User: RW
               (1 << MPU_RASR_B_Pos)     | 
               (28 << MPU_RASR_SIZE_Pos) | // 2^29 = 512MB
               (1 << MPU_RASR_ENABLE_Pos);

    /* --- REGION 3: SYSTEM PPB (Bảo vệ Lõi) ---
     * Base: 0xE0000000 (Chứa NVIC, SysTick, SCB...)
     * Attr: Privileged Only (AP=1)
     * Mục đích: Cấm User Task can thiệp vào ngắt hoặc reset chip.
     */
    MPU_RNR  = R_SYSTEM;
    MPU_RBAR = 0xE0000000;
    MPU_RASR = (1 << MPU_RASR_XN_Pos)    |
               (1 << MPU_RASR_AP_Pos)    | // Priv: RW, User: NO ACCESS
               (28 << MPU_RASR_SIZE_Pos) | 
               (1 << MPU_RASR_ENABLE_Pos);

    /* --- REGION 4: NULL POINTER GUARD (Bẫy lỗi) ---
     * Base: 0x00000000, Size: 32 Bytes
     * Attr: No Access All (AP=0)
     * Mục đích: Vùng này đè lên đầu Flash. Truy cập 0x0 sẽ gây lỗi ngay.
     */
    MPU_RNR  = R_NULL_GUARD;
    MPU_RBAR = 0x00000000;
    MPU_RASR = (1 << MPU_RASR_XN_Pos)    |
               (0 << MPU_RASR_AP_Pos)    | // NO ACCESS
               (4 << MPU_RASR_SIZE_Pos)  | // 2^5 = 32 Bytes
               (1 << MPU_RASR_ENABLE_Pos);

    /* --- REGION 7: DMA CONTROL TABLE (Bảo vệ phần cứng DMA) ---
     * Base: Địa chỉ biến dma_table (phải align 1024)
     * Size: 1KB (Cho 32 kênh DMA)
     * Attr: Privileged RW, User Read-Only (AP=2)
     * Mục đích: User xem được trạng thái DMA nhưng không được ghi bậy làm treo DMA.
     */
    MPU_RNR  = R_DMA;
    MPU_RBAR = (uint32_t)dma_table; 
    MPU_RASR = (1 << MPU_RASR_XN_Pos)    |
               (2 << MPU_RASR_AP_Pos)    | // Priv: RW, User: READ-ONLY
               (1 << MPU_RASR_C_Pos)     | 
               (1 << MPU_RASR_S_Pos)     |
               (9 << MPU_RASR_SIZE_Pos)  | // 2^10 = 1KB
               (1 << MPU_RASR_ENABLE_Pos);

    /* Kích hoạt MemManage Fault và MPU */
    SCB_SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
    
    /* ENABLE: Bật MPU. PRIVDEFENA: Code Kernel được truy cập vùng chưa định nghĩa */
    MPU_CTRL = MPU_CTRL_ENABLE_Msk | MPU_CTRL_PRIVDEFENA_Msk;
    
    __DSB();
    __ISB();
}

/* ----------------------------------------------------------------------------
 * Hàm cấu hình MPU cho Task (Gọi mỗi khi Context Switch)
 * Thiết lập các vùng động (Stack, Heap)
 * ---------------------------------------------------------------------------- */
void mpu_config_for_task(PCB_t *task) {
    /* Tắt MPU tạm thời để cấu hình */
    MPU_CTRL = 0;
    __DSB();

    /* --- REGION 5: TASK STACK ---
     * Bảo vệ Stack riêng của Task.
     * Sử dụng hàm mpu_calc_region_size từ mpu.h
     */
    MPU_RNR = R_STACK;
    // Thêm & ~0x1F để ép 5 bit cuối về 0 (Xóa cờ VALID)
    MPU_RBAR = ((uint32_t)task->stack_base) & ~0x1F;
    
    uint32_t s_size = mpu_calc_region_size(task->stack_size);
    
    MPU_RASR = (1 << MPU_RASR_XN_Pos)    | // Stack No-Exec
               (3 << MPU_RASR_AP_Pos)    | // Priv: RW, User: RW
               (1 << MPU_RASR_C_Pos)     | 
               (1 << MPU_RASR_S_Pos)     |
               (s_size << MPU_RASR_SIZE_Pos) |
               (1 << MPU_RASR_ENABLE_Pos);

    /* --- REGION 6: TASK HEAP (Nếu có) ---
     * Bảo vệ vùng nhớ động riêng của Task.
     */
    if (task->heap_base != 0 && task->heap_size > 0) {
        MPU_RNR = R_HEAP;
        // Thêm & ~0x1F để ép 5 bit cuối về 0
        MPU_RBAR = ((uint32_t)task->heap_base) & ~0x1F;
        
        uint32_t h_size = mpu_calc_region_size(task->heap_size);

        MPU_RASR = (1 << MPU_RASR_XN_Pos) | 
                   (3 << MPU_RASR_AP_Pos) | // Priv: RW, User: RW
                   (1 << MPU_RASR_C_Pos)  | 
                   (1 << MPU_RASR_S_Pos)  |
                   (h_size << MPU_RASR_SIZE_Pos) |
                   (1 << MPU_RASR_ENABLE_Pos);
    } else {
        /* Nếu Task không dùng Heap, TẮT Region này để an toàn */
        MPU_RNR = R_HEAP;
        /* Chỉ xóa bit số 0 (ENABLE), giữ nguyên các bit khác để phần cứng không nổi điên */
        MPU_RASR &= ~(1 << MPU_RASR_ENABLE_Pos); 
    }

    /* Bật lại MPU */
    MPU_CTRL = MPU_CTRL_ENABLE_Msk | MPU_CTRL_PRIVDEFENA_Msk;
    __DSB();
    __ISB();
}

/* ----------------------------------------------------------------------------
 * MemManage Fault Handler (Màn hình xanh báo lỗi)
 * ---------------------------------------------------------------------------- */
void MemManage_Handler(void) {
    /* Tắt MPU để Kernel có thể in log mà không bị lỗi chồng lỗi */
    MPU_CTRL = 0; 
    
    uart_print("\r\n\033[1;31m[CRITICAL] *** MEMORY PROTECTION VIOLATION ***\033[0m\r\n");
    
    uint32_t fault_addr = SCB_MMFAR;
    uint32_t mmfsr = SCB_CFSR & 0xFF;

    /* Phân tích nguyên nhân lỗi */
    if (mmfsr & (1 << 7)) { // Kiểm tra bit MMARVALID
        uart_print("Violation Address: 0x");
        uart_print_hex32(fault_addr);
        uart_print("\r\n");

        if (fault_addr < 0x20) {
            uart_print("=> CAUSE: NULL Pointer Dereference (Accessing 0x0)\r\n");
        } else if (fault_addr >= 0xE0000000) {
            uart_print("=> CAUSE: Illegal Access to System Registers (NVIC/SCB)\r\n");
        } else if ((uint32_t)dma_table <= fault_addr && fault_addr < (uint32_t)dma_table + 1024) {
            uart_print("=> CAUSE: Illegal Write to DMA Control Table\r\n");
        } else if (fault_addr < 0x20000000) {
            uart_print("=> CAUSE: User tried to WRITE to Flash Memory\r\n");
        } else {
            uart_print("=> CAUSE: Stack Overflow or Unauthorized RAM Access\r\n");
        }
    } else {
        uart_print("=> CAUSE: Instruction Fetch Violation or Stacking Error\r\n");
    }

    /* Xử lý Task gây lỗi */
    if (current_pcb) {
        uart_print("Killing Offending Task PID: ");
        uart_print_dec(current_pcb->pid);
        uart_print("\r\n");
        
        current_pcb->state = PROC_SUSPENDED; // Treo vĩnh viễn task lỗi
    }

    /* Xóa cờ lỗi để tránh lặp vô tận */
    SCB_CFSR |= 0xFF;

    uart_print("Action: Task Suspended. Triggering Context Switch...\r\n");

    /* Kích hoạt PendSV để chuyển sang Task khác ngay lập tức */
    *(volatile uint32_t *)0xE000ED04 = (1 << 28); 
    
    /* Vòng lặp an toàn (nếu PendSV chưa kịp chạy) */
    while(1); 
}