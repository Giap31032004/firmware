#include "task.h"
#include "dma.h"
#include "uart.h"

/* Buffer nguồn và đích */
static char src_buffer[] = "Hello DMA! This string is copied by Hardware.";
static char dst_buffer[64];

/* Hàm delay phụ trợ */
static void delay_loop(volatile uint32_t count) {
    while(count--) { __asm("nop"); }
}

void task_dma_test(void) {
    uart_print("[DMA] Task Started.\n");
    
    /* 1. Init DMA */
    dma_init();
    
    /* Xóa buffer đích để chắc chắn */
    for(int i=0; i<64; i++) dst_buffer[i] = 0;

    while(1) {
        uart_print("[DMA] Copying data...\n");

        /* 2. Thực hiện Copy bằng DMA */
        // Copy 46 bytes từ src sang dst
        bool res = dma_memcpy(src_buffer, dst_buffer, sizeof(src_buffer));
        
        if (res) {
            /* Vì là DMA Software mode (Auto), nó chạy rất nhanh */
            // In kết quả
            uart_print("[DMA] Result in dst_buffer: ");
            uart_print(dst_buffer);
            uart_print("\n");
        } 
        else {
            uart_print("[DMA] Failed to setup DMA transfer.\n");
        }

        // Nghỉ 5 giây
        delay_loop(25000000); 
    }
}