#ifndef MYOS_KERNEL_TICK_H
#define MYOS_KERNEL_TICK_H

#include <stdint.h>

typedef struct {
    uint32_t wakeup_tick;
} timer_info_t;

static inline int tick_after_or_equal(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

void kernel_tick(void);

#endif /* MYOS_KERNEL_TICK_H */
