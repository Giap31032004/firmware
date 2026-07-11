#include "port.h"
#include "hardware_config.h"
#include "kernel.h"
#include "nvic.h"
#include "systick.h"
#include "stm32f407xx.h"

void port_disable_irq(void)
{
    __disable_irq();
}

void port_enable_irq(void)
{
    __enable_irq();
}

uint32_t port_get_irq_state(void)
{
    return __get_PRIMASK();
}

void port_set_irq_state(uint32_t state)
{
    __set_PRIMASK(state);
}

int port_in_isr(void)
{
    return __get_IPSR() != 0U;
}

void port_yield(void)
{
    nvic_set_pending(IRQ_PENDSV);
}

void port_yield_from_isr(void)
{
    nvic_set_pending(IRQ_PENDSV);
}

void port_start_scheduler(uint32_t *first_sp)
{
    (void)first_sp;

    nvic_set_irq_priority(IRQ_PENDSV, IRQ_PRIO_PENDSV);
    nvic_set_irq_priority(IRQ_SYSTICK, IRQ_PRIO_SYSTICK);
    nvic_set_irq_priority(IRQ_SVC, IRQ_PRIO_SVC);

    if (port_tick_start_periodic() != 0) {
        kernel_panic("systick init failed", __FILE__, __LINE__);
    }

    __asm volatile ("svc #0");

    for (;;) {
    }
}

int port_tick_start_periodic(void)
{
    return systick_init(SYSTICK_RATE_HZ);
}

uint32_t port_runtime_counter(void)
{
    uint32_t tick = os_tick_count;
    uint32_t load = SysTick->LOAD + 1U;
    uint32_t val = SysTick->VAL;
    uint32_t elapsed_in_tick = 0U;
    const uint32_t cycles_per_tick = CPU_CLOCK_HZ / SYSTICK_RATE_HZ;

    if (load > 1U && val <= load) {
        elapsed_in_tick = load - val;
        if (elapsed_in_tick > cycles_per_tick) {
            elapsed_in_tick = cycles_per_tick;
        }
    }

    return (tick * cycles_per_tick) + elapsed_in_tick;
}

void port_tick_stop(void)
{
    systick_disable();
}

void port_tick_clear_pending(void)
{
    systick_clear_pending();
}

uint32_t port_tick_start_oneshot(uint32_t ticks)
{
    return systick_start_oneshot(ticks);
}

uint32_t port_tick_elapsed(uint32_t programmed_ticks)
{
    return systick_elapsed_ticks(programmed_ticks);
}

void port_system_reset(void)
{
    __DSB();
    SCB->AIRCR = (0x5FAUL << SCB_AIRCR_VECTKEY_Pos) |
                 SCB_AIRCR_SYSRESETREQ_Msk;
    __DSB();

    for (;;) {
    }
}
