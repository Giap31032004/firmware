#ifndef MYOS_KERNEL_RUNTIME_STATS_H
#define MYOS_KERNEL_RUNTIME_STATS_H

#include <stdint.h>
#include "task.h"

void runtime_stats_init(void);
uint32_t runtime_stats_counter(void);
void runtime_stats_task_switched_in(TCB_t *task);
void runtime_stats_task_switched_out(TCB_t *task);
void runtime_stats_print(void);

#endif /* MYOS_KERNEL_RUNTIME_STATS_H */
