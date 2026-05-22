#include <stdint.h>
#include "systick.h"
#include "hardware_config.h"

#define SYSTICK_BASE    0xE000E010U

#define SYSTICK_CTRL    (*(volatile uint32_t *)(SYSTICK_BASE + 0x00))
#define SYSTICK_LOAD    (*(volatile uint32_t *)(SYSTICK_BASE + 0x04))
#define SYSTICK_VAL     (*(volatile uint32_t *)(SYSTICK_BASE + 0x08))

/* Bit definitions */
#define SYSTICK_CTRL_ENABLE    (1U << 0)
#define SYSTICK_CTRL_TICKINT   (1U << 1)
#define SYSTICK_CTRL_CLKSOURCE (1U << 2)

void systick_init(uint32_t tick_hz) {

    uint32_t reload;

    /* 0. Validate input */
    if (tick_hz == 0U) {
        return;
    }

    /* 1. Disable SysTick */
    SYSTICK_CTRL = 0;

    /* 2. Calculate reload */
    reload = (CPU_CLOCK_HZ / tick_hz) - 1U;

    /* 3. Check 24-bit limit */
    if (reload > 0xFFFFFFU) {
        return;
    }

    /* 4. Set reload */
    SYSTICK_LOAD = reload;

    /* 5. Reset counter */
    SYSTICK_VAL = 0U;

    /* 6. Enable SysTick */
    SYSTICK_CTRL =
        SYSTICK_CTRL_CLKSOURCE |
        SYSTICK_CTRL_TICKINT   |
        SYSTICK_CTRL_ENABLE;

    /* 7. Ensure effect */
    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");
}
