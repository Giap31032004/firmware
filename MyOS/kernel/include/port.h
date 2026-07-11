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

/* =========================================================
 * SYSTEM TICK
 * ========================================================= */

int port_tick_start_periodic(void);
void port_tick_stop(void);
void port_tick_clear_pending(void);
uint32_t port_tick_start_oneshot(uint32_t ticks);
uint32_t port_tick_elapsed(uint32_t programmed_ticks);
uint32_t port_runtime_counter(void);

#endif /* PORT_H */
