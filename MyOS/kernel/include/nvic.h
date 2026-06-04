#ifndef MYOS_NVIC_H
#define MYOS_NVIC_H

#include <stdint.h>

#define NVIC_PRIO_BITS 4U

typedef int32_t irq_num_t;

enum {
    IRQ_SYSTICK = -1,
    IRQ_PENDSV = -2,
    IRQ_SVC = -5
};

void nvic_enable_irq(irq_num_t irq);
void nvic_disable_irq(irq_num_t irq);
void nvic_set_pending(irq_num_t irq);
void nvic_clear_pending(irq_num_t irq);
uint32_t nvic_get_pending(irq_num_t irq);
uint32_t nvic_get_active(irq_num_t irq);
void nvic_set_irq_priority(irq_num_t irq, uint32_t irq_priority);
uint32_t nvic_get_irq_priority(irq_num_t irq);

/* Backward-compatible aliases. New code should use *_irq_priority names. */
void nvic_set_priority(irq_num_t irq, uint32_t irq_priority);
uint32_t nvic_get_priority(irq_num_t irq);

#endif /* MYOS_NVIC_H */
