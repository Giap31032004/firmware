#include "systick.h"
#include "kernel.h"
#include "uart.h"

#define SYSTICK_BASE   0xE000E010
#define SYSTICK_CTRL   (*(volatile uint32_t*)(SYSTICK_BASE + 0x00))
#define SYSTICK_LOAD   (*(volatile uint32_t*)(SYSTICK_BASE + 0x04))
#define SYSTICK_VAL    (*(volatile uint32_t*)(SYSTICK_BASE + 0x08))
#define ICSR           (*(volatile uint32_t*)(0xE000ED04))
#define PENDSVSET      (1 << 28)

void systick_init(uint32_t ticks) 
{
    SYSTICK_LOAD = ticks - 1;
    SYSTICK_VAL  = 0;
    SYSTICK_CTRL = 0x07;  // enable, interrupt, processor clock
}

void SysTick_Handler(void) 
{
    static int cnt = 0;
    cnt++;
    // Lightweight debug: print a dot every 10 ticks to avoid flooding
    if ((cnt % 10) == 0) {
        uart_putc_raw('.');
    }

    // cập nhật giờ đánh thức
    process_timer_tick();

    process_schedule();
    ICSR |= PENDSVSET; // set cờ PendSV
}
