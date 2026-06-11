#ifndef MYOS_KERNEL_SCHEDULER_H
#define MYOS_KERNEL_SCHEDULER_H

#include <stdint.h>
#include "kernel_config.h"
#include "list.h"

typedef struct TCB TCB_t;

#define PRIORITY_IDLE 0
#define PRIORITY_LOW 1
#define PRIORITY_NORMAL 3
#define PRIORITY_HIGH 5
#define PRIORITY_REALTIME (MAX_PRIORITY - 1)

typedef struct
{
    uint8_t priority;
    uint8_t base_priority;
    uint32_t remaining_ticks;
} scheduler_info_t;

extern TCB_t *current_tcb;
extern TCB_t *next_tcb;
extern list_t ready_list[MAX_PRIORITY];
extern uint32_t top_ready_priority_bitmap;

void scheduler_init(void);
void scheduler_lock(void);
void scheduler_unlock(void);
void scheduler_yield_if_needed(void);
void scheduler_tick(void);
void os_schedule(void);
void os_start(void);

void add_task_to_ready_queue(TCB_t *task);
void remove_task_from_ready_queue(TCB_t *task);
TCB_t *get_highest_priority_ready_task(void);

#endif /* MYOS_KERNEL_SCHEDULER_H */
