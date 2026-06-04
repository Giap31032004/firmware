#include "nvic.h"

#define NVIC_BASE_ADDR     0xE000E100UL
#define SCB_BASE_ADDR      0xE000ED00UL

#define NVIC_ISER          ((volatile uint32_t *)(NVIC_BASE_ADDR + 0x000UL))
#define NVIC_ICER          ((volatile uint32_t *)(NVIC_BASE_ADDR + 0x080UL))
#define NVIC_ISPR          ((volatile uint32_t *)(NVIC_BASE_ADDR + 0x100UL))
#define NVIC_ICPR          ((volatile uint32_t *)(NVIC_BASE_ADDR + 0x180UL))
#define NVIC_IABR          ((volatile uint32_t *)(NVIC_BASE_ADDR + 0x200UL))
#define NVIC_IPR           ((volatile uint8_t  *)(NVIC_BASE_ADDR + 0x300UL))

#define SCB_ICSR           (*(volatile uint32_t *)(SCB_BASE_ADDR + 0x04UL))
#define SCB_SHPR           ((volatile uint8_t *)(SCB_BASE_ADDR + 0x18UL))

#define SCB_ICSR_PENDSVSET     (1UL << 28)
#define SCB_ICSR_PENDSVCLR     (1UL << 27)
#define SCB_ICSR_PENDSTSET     (1UL << 26)
#define SCB_ICSR_PENDSTCLR     (1UL << 25)

static uint32_t encode_irq_priority(uint32_t irq_priority)
{
    uint32_t max_priority = (1UL << NVIC_PRIO_BITS) - 1UL;

    if (irq_priority > max_priority) {
        irq_priority = max_priority;
    }

    return irq_priority << (8U - NVIC_PRIO_BITS);
}

static int system_priority_index(irq_num_t irq, uint32_t *index)
{
    if (irq < -12 || irq >= 0 || index == 0) {
        return 0;
    }

    *index = (uint32_t)(irq + 12);
    return 1;
}

void nvic_enable_irq(irq_num_t irq)
{
    if (irq < 0) {
        return;
    }

    NVIC_ISER[((uint32_t)irq) >> 5U] = 1UL << (((uint32_t)irq) & 0x1FU);
}

void nvic_disable_irq(irq_num_t irq)
{
    if (irq < 0) {
        return;
    }

    NVIC_ICER[((uint32_t)irq) >> 5U] = 1UL << (((uint32_t)irq) & 0x1FU);
}

void nvic_set_pending(irq_num_t irq)
{
    if (irq >= 0) {
        NVIC_ISPR[((uint32_t)irq) >> 5U] = 1UL << (((uint32_t)irq) & 0x1FU);
    } else if (irq == IRQ_PENDSV) {
        SCB_ICSR = SCB_ICSR_PENDSVSET;
    } else if (irq == IRQ_SYSTICK) {
        SCB_ICSR = SCB_ICSR_PENDSTSET;
    }
}

void nvic_clear_pending(irq_num_t irq)
{
    if (irq >= 0) {
        NVIC_ICPR[((uint32_t)irq) >> 5U] = 1UL << (((uint32_t)irq) & 0x1FU);
    } else if (irq == IRQ_PENDSV) {
        SCB_ICSR = SCB_ICSR_PENDSVCLR;
    } else if (irq == IRQ_SYSTICK) {
        SCB_ICSR = SCB_ICSR_PENDSTCLR;
    }
}

uint32_t nvic_get_pending(irq_num_t irq)
{
    if (irq >= 0) {
        uint32_t value = NVIC_ISPR[((uint32_t)irq) >> 5U];
        return (value >> (((uint32_t)irq) & 0x1FU)) & 1UL;
    }

    if (irq == IRQ_PENDSV) {
        return (SCB_ICSR >> 28U) & 1UL;
    }

    if (irq == IRQ_SYSTICK) {
        return (SCB_ICSR >> 26U) & 1UL;
    }

    return 0;
}

uint32_t nvic_get_active(irq_num_t irq)
{
    if (irq < 0) {
        return 0;
    }

    uint32_t value = NVIC_IABR[((uint32_t)irq) >> 5U];
    return (value >> (((uint32_t)irq) & 0x1FU)) & 1UL;
}

void nvic_set_irq_priority(irq_num_t irq, uint32_t irq_priority)
{
    uint8_t encoded = (uint8_t)encode_irq_priority(irq_priority);

    if (irq >= 0) {
        NVIC_IPR[(uint32_t)irq] = encoded;
        return;
    }

    uint32_t index;
    if (system_priority_index(irq, &index)) {
        SCB_SHPR[index] = encoded;
    }
}

uint32_t nvic_get_irq_priority(irq_num_t irq)
{
    uint32_t encoded;

    if (irq >= 0) {
        encoded = NVIC_IPR[(uint32_t)irq];
        return encoded >> (8U - NVIC_PRIO_BITS);
    }

    uint32_t index;
    if (system_priority_index(irq, &index)) {
        encoded = SCB_SHPR[index];
        return encoded >> (8U - NVIC_PRIO_BITS);
    }

    return 0;
}

void nvic_set_priority(irq_num_t irq, uint32_t irq_priority)
{
    nvic_set_irq_priority(irq, irq_priority);
}

uint32_t nvic_get_priority(irq_num_t irq)
{
    return nvic_get_irq_priority(irq);
}
