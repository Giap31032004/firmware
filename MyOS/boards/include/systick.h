#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>
#include "kernel.h"

os_status_t systick_init(uint32_t tick_hz);
void systick_disable(void);
void systick_clear_pending(void);
uint32_t systick_max_suppressed_ticks(void);
uint32_t systick_start_oneshot(uint32_t ticks);
uint32_t systick_elapsed_ticks(uint32_t programmed_ticks);

#endif /* SYSTICK_H */
