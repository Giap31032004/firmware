#ifndef MYOS_KERNEL_TICK_H
#define MYOS_KERNEL_TICK_H

#include<stdint.h>

typedef struct {
    uint32_t wakeup_tick;
} timer_info_t;

void kernel_tick(void);

#endif /* MYOS_KERNEL_TICK_H */
