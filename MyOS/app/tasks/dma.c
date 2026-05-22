#include "dma.h"
#include "kernel.h"
#include "task.h"
#include "uart.h"

static char src_buffer[] = "Hello DMA! This string is copied by hardware.";
static char dst_buffer[64];

void task_dma_test(void)
{
    uart_print("[DMA] Task started.\r\n");
    dma_init();

    for (int i = 0; i < 64; i++) {
        dst_buffer[i] = 0;
    }

    while (1) {
        if (dma_memcpy(src_buffer, dst_buffer, sizeof(src_buffer))) {
            uart_print("[DMA] Copy OK: ");
            uart_print(dst_buffer);
            uart_print("\r\n");
        } else {
            uart_print("[DMA] Failed to setup transfer.\r\n");
        }

        os_delay(5000);
    }
}
