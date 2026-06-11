#include "systick.h"

#include "hardware_config.h"
#include "stm32f407xx.h"

#define SYSTICK_CYCLES_PER_TICK (CPU_CLOCK_HZ / SYSTICK_RATE_HZ)

void systick_disable(void)
{
    SysTick->CTRL = 0U;
    SysTick->VAL = 0U;

    __DSB();
    __ISB();
}

void systick_clear_pending(void)
{
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
}

uint32_t systick_max_suppressed_ticks(void)
{
    if (SYSTICK_CYCLES_PER_TICK == 0U) {
        return 0U;
    }

    return SysTick_LOAD_RELOAD_Msk / SYSTICK_CYCLES_PER_TICK;
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

    SysTick->LOAD = reload;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;

    __DSB();
    __ISB();

    return ticks;
}

uint32_t systick_elapsed_ticks(uint32_t programmed_ticks)
{
    uint32_t ctrl = SysTick->CTRL;
    uint32_t reload_cycles = SysTick->LOAD + 1U;
    uint32_t elapsed_cycles;
    uint32_t elapsed_ticks;

    if (programmed_ticks == 0U || SYSTICK_CYCLES_PER_TICK == 0U) {
        return 0U;
    }

    if ((ctrl & SysTick_CTRL_COUNTFLAG_Msk) != 0U) {
        return programmed_ticks;
    }

    elapsed_cycles =
        reload_cycles - (SysTick->VAL & SysTick_VAL_CURRENT_Msk);
    elapsed_ticks = elapsed_cycles / SYSTICK_CYCLES_PER_TICK;

    if (elapsed_ticks > programmed_ticks) {
        elapsed_ticks = programmed_ticks;
    }

    return elapsed_ticks;
}

int systick_init(uint32_t tick_hz)
{
    uint32_t reload;

    if (tick_hz == 0U || tick_hz > CPU_CLOCK_HZ) {
        systick_disable();
        return -1;
    }

    reload = (CPU_CLOCK_HZ / tick_hz) - 1U;
    if (reload > SysTick_LOAD_RELOAD_Msk) {
        systick_disable();
        return -1;
    }

    systick_disable();

    SysTick->LOAD = reload;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;

    __DSB();
    __ISB();

    return 0;
}
