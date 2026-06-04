#ifndef PORT_H
#define PORT_H

#include <stdint.h>

/* =========================================================
 * IRQ CONTROL PRIMITIVES
 * ========================================================= */

void port_disable_irq(void);
void port_enable_irq(void);
uint32_t port_get_irq_state(void);
void port_set_irq_state(uint32_t state);
int port_in_isr(void);

/* =========================================================
 * CONTEXT SWITCH
 * ========================================================= */

void port_yield(void);
void port_yield_from_isr(void);
void port_start_scheduler(uint32_t *first_sp);
void port_system_reset(void);

#endif /* PORT_H */
