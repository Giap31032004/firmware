#include <stdint.h>
#include "scb.h"

#define FLASH_BASE_ADDR  0x08000000U

#define SCB_BASE        0xE000ED00U

#define SCB_CPACR       (*(volatile uint32_t *)(SCB_BASE + 0x88))
#define SCB_VTOR        (*(volatile uint32_t *)(SCB_BASE + 0x08))

#define SCB_CPACR_CP10_FULL    (0x3U << 20)
#define SCB_CPACR_CP11_FULL    (0x3U << 22)

#define FPU_FPCCR (*(volatile uint32_t *)0xE000EF34)

#define BARRIER() \
    __asm volatile ("dsb 0xF" ::: "memory"); \
    __asm volatile ("isb 0xF" ::: "memory")

void scb_init(void) {

    /* 1. Enable FPU */
    SCB_CPACR |= (SCB_CPACR_CP10_FULL | SCB_CPACR_CP11_FULL);
    BARRIER();

    /* 2. Enable lazy stacking */
    FPU_FPCCR |= ((1U << 31) | (1U << 30));
    BARRIER();

    /* 3. Set vector table */
    extern uint32_t _isr_vector;
    SCB_VTOR = (uint32_t)&_isr_vector;
    BARRIER();
}