#ifndef MYOS_KERNEL_TIMER_H
#define MYOS_KERNEL_TIMER_H

#include <stdint.h>

#include "kernel.h"
#include "list.h"

typedef void (*os_timer_callback_t)(void *arg);

typedef struct os_timer {
    list_node_t node;
    os_timer_callback_t callback;
    void *arg;
    uint32_t expiry_tick;
    uint32_t period_ticks;
    uint8_t active;
    uint8_t auto_reload;
} os_timer_t;

void timer_init(void);
void timer_process_tick(void);
uint32_t timer_ticks_until_next_expiry(void);
void os_timer_init(os_timer_t *timer,
                   os_timer_callback_t callback,
                   void *arg);
os_status_t os_timer_start(os_timer_t *timer,
                           uint32_t delay_ticks);
os_status_t os_timer_start_periodic(os_timer_t *timer,
                                    uint32_t period_ticks);
void os_timer_stop(os_timer_t *timer);

#endif /* MYOS_KERNEL_TIMER_H */
