#include "port.h"
#include "hardware_config.h"
#include "systick.h"
#include "stm32f407xx.h"

extern void start_first_task(uint32_t *first_sp);

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

void port_yield(void)
{
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}

void port_yield_from_isr(void)
{
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}

void port_start_scheduler(uint32_t *first_sp)
{
    NVIC_SetPriority(PendSV_IRQn, PENDSV_IRQ_PRIORITY);
    NVIC_SetPriority(SysTick_IRQn, SYSTICK_IRQ_PRIORITY);
    NVIC_SetPriority(SVCall_IRQn, SVC_IRQ_PRIORITY);

    systick_init(SYSTICK_RATE_HZ);
    start_first_task(first_sp);
}
