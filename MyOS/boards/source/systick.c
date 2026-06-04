#include "systick.h"
#include "hardware_config.h"

#define SYSTICK_BASE            0xE000E010UL

#define SYSTICK_CTRL            (*(volatile uint32_t *)(SYSTICK_BASE + 0x00UL))
#define SYSTICK_LOAD            (*(volatile uint32_t *)(SYSTICK_BASE + 0x04UL))
#define SYSTICK_VAL             (*(volatile uint32_t *)(SYSTICK_BASE + 0x08UL))

#define SYSTICK_CTRL_ENABLE     (1U << 0)
#define SYSTICK_CTRL_TICKINT    (1U << 1)
#define SYSTICK_CTRL_CLKSOURCE  (1U << 2)
#define SYSTICK_CTRL_COUNTFLAG  (1U << 16)
#define SYSTICK_LOAD_MAX        0x00FFFFFFUL
#define SCB_ICSR                (*(volatile uint32_t *)0xE000ED04UL)
#define SCB_ICSR_PENDSTCLR      (1UL << 25)

#define SYSTICK_CYCLES_PER_TICK (CPU_CLOCK_HZ / SYSTICK_RATE_HZ)

void systick_disable(void)
{
    SYSTICK_CTRL = 0U;
    SYSTICK_VAL = 0U;

    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");
}

void systick_clear_pending(void)
{
    SCB_ICSR = SCB_ICSR_PENDSTCLR;
}

uint32_t systick_max_suppressed_ticks(void)
{
    if (SYSTICK_CYCLES_PER_TICK == 0U) {
        return 0U;
    }

    return SYSTICK_LOAD_MAX / SYSTICK_CYCLES_PER_TICK;
}

uint32_t systick_start_oneshot(uint32_t ticks)
{
    uint32_t max_ticks = systick_max_suppressed_ticks();
    uint32_t reload;

    if (ticks == 0U || max_ticks == 0U) {
        systick_disable();
        return 0U;
    }

    if (ticks > max_ticks) {
        ticks = max_ticks;
    }

    reload = (ticks * SYSTICK_CYCLES_PER_TICK) - 1U;

    systick_disable();
    systick_clear_pending();

    SYSTICK_LOAD = reload;
    SYSTICK_VAL = 0U;
    SYSTICK_CTRL = SYSTICK_CTRL_CLKSOURCE |
                   SYSTICK_CTRL_TICKINT |
                   SYSTICK_CTRL_ENABLE;

    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");

    return ticks;
}

uint32_t systick_elapsed_ticks(uint32_t programmed_ticks)
{
    uint32_t ctrl = SYSTICK_CTRL;
    uint32_t reload_cycles = SYSTICK_LOAD + 1U;
    uint32_t elapsed_cycles;
    uint32_t elapsed_ticks;

    if (programmed_ticks == 0U || SYSTICK_CYCLES_PER_TICK == 0U) {
        return 0U;
    }

    if ((ctrl & SYSTICK_CTRL_COUNTFLAG) != 0U) {
        return programmed_ticks;
    }

    elapsed_cycles = reload_cycles - (SYSTICK_VAL & SYSTICK_LOAD_MAX);
    elapsed_ticks = elapsed_cycles / SYSTICK_CYCLES_PER_TICK;

    if (elapsed_ticks > programmed_ticks) {
        elapsed_ticks = programmed_ticks;
    }

    return elapsed_ticks;
}

os_status_t systick_init(uint32_t tick_hz)
{
    uint32_t reload;

    if (tick_hz == 0U || tick_hz > CPU_CLOCK_HZ) {
        systick_disable();
        return OS_ERROR;
    }

    reload = (CPU_CLOCK_HZ / tick_hz) - 1U;
    if (reload > SYSTICK_LOAD_MAX) {
        systick_disable();
        return OS_ERROR;
    }

    systick_disable();

    SYSTICK_LOAD = reload;
    SYSTICK_VAL = 0U;
    SYSTICK_CTRL = SYSTICK_CTRL_CLKSOURCE |
                   SYSTICK_CTRL_TICKINT |
                   SYSTICK_CTRL_ENABLE;

    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");

    return OS_OK;
}
