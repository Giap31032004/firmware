#ifndef MYOS_KERNEL_TICK_H
#define MYOS_KERNEL_TICK_H

#include <stdint.h>

typedef struct {
    uint32_t wakeup_tick;
} task_time_info_t;

extern volatile uint32_t os_tick_count;

static inline int tick_after_or_equal(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

void kernel_tick(void);
void tick_init(void);
void tick_step(uint32_t ticks);

#endif /* MYOS_KERNEL_TICK_H */
