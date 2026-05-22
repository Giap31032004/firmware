#include "rcc.h"
#include<stdint.h>

#define RCC_BASE        0x40023800U

#define RCC_CR          (*(volatile uint32_t *)(RCC_BASE + 0x00))
#define RCC_PLLCFGR     (*(volatile uint32_t *)(RCC_BASE + 0x04))
#define RCC_CFGR        (*(volatile uint32_t *)(RCC_BASE + 0x08))

#define RCC_CR_HSION    (1U << 0)
#define RCC_CR_HSIRDY   (1U << 1)

#define RCC_CR_PLLON    (1U << 24)
#define RCC_CR_PLLRDY   (1U << 25)

#define RCC_CFGR_SW_MASK   (0x3U)
#define RCC_CFGR_SW_HSI    (0x0U)

#define RCC_CFGR_SWS_MASK  (0x3U << 2)
#define RCC_CFGR_SWS_HSI   (0x0U << 2)

#define RCC_TIMEOUT     100000U

#define BARRIER() \
    __asm volatile ("dsb 0xF" ::: "memory"); \
    __asm volatile ("isb 0xF" ::: "memory")

void rcc_init(void) {

    uint32_t timeout;

    /* 1. Enable HSI */
    RCC_CR |= RCC_CR_HSION;

    /* 2. Wait HSIRDY */
    timeout = RCC_TIMEOUT;
    while (!(RCC_CR & RCC_CR_HSIRDY) && timeout--);

    /* 3. Select HSI */
    RCC_CFGR = (RCC_CFGR & ~RCC_CFGR_SW_MASK) | RCC_CFGR_SW_HSI;

    /* 4. Verify switch */
    timeout = RCC_TIMEOUT;
    while (((RCC_CFGR & RCC_CFGR_SWS_MASK) != RCC_CFGR_SWS_HSI) && timeout--);
    BARRIER();
    /* 5. Disable PLL */
    RCC_CR &= ~RCC_CR_PLLON;

    timeout = RCC_TIMEOUT;
    while ((RCC_CR & RCC_CR_PLLRDY) && timeout--);

    /* 6. Reset PLL */
    RCC_PLLCFGR = 0x24003010U;
}