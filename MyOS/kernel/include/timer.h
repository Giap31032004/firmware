#ifndef MYOS_KERNEL_TIMER_H
#define MYOS_KERNEL_TIMER_H

#include <stdint.h>

#include "kernel.h"
#include "list.h"

typedef void (*os_timer_callback_t)(void *arg);

typedef enum {
    OS_TIMER_ONESHOT = 0,
    OS_TIMER_PERIODIC
} os_timer_mode_t;

typedef struct os_timer {
    list_node_t node;
    os_timer_callback_t callback;
    void *arg;
    uint32_t period_ticks;
    uint32_t expiry_tick;
    os_timer_mode_t mode;
    uint8_t active;
    uint8_t in_callback;
    uint8_t reload_cancelled;
} os_timer_t;

void timer_init(void);
void timer_process_tick(void);
uint32_t timer_ticks_until_next_expiry(void);
void os_timer_init(os_timer_t *timer,
                   os_timer_callback_t callback,
                   void *arg);
os_status_t os_timer_start(os_timer_t *timer,
                           uint32_t period_ticks,
                           os_timer_mode_t mode);
void os_timer_stop(os_timer_t *timer);
void os_timer_reset(os_timer_t *timer);
uint8_t os_timer_is_active(const os_timer_t *timer);

#endif /* MYOS_KERNEL_TIMER_H */
