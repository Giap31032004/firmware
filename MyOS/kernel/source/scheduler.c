#include <stddef.h>
#include <stdint.h>

#include "critical.h"
#include "scheduler.h"
#include "task.h"
#include "port.h"

ready_list_t ready_list[MAX_PRIORITY];
uint32_t top_ready_priority_bitmap = 0;

TCB_t *current_tcb = NULL;
TCB_t *next_tcb = NULL;
static uint32_t scheduler_lock_count = 0;
static uint8_t scheduler_yield_pending = 0;

void scheduler_init_queues(void)
{
    for (int i = 0; i < MAX_PRIORITY; i++) {
        list_init(&ready_list[i]);
    }
    top_ready_priority_bitmap = 0;
    current_tcb = NULL;
    next_tcb = NULL;
}

void scheduler_lock(void)
{
    uint32_t irq_state = os_enter_critical();
    scheduler_lock_count++;
    os_exit_critical(irq_state);
}

void scheduler_unlock(void)
{
    uint32_t irq_state = os_enter_critical();

    if (scheduler_lock_count > 0) {
        scheduler_lock_count--;
    }

    int should_yield = (scheduler_lock_count == 0 && scheduler_yield_pending);
    if (should_yield) {
        scheduler_yield_pending = 0;
    }

    os_exit_critical(irq_state);

    if (should_yield) {
        os_schedule();
    }
}

void add_task_to_ready_queue(TCB_t *p)
{
    if (p == NULL) {
        return;
    }

    if (p->state == TASK_READY ||
        p->state == TASK_UNUSED ||
        p->state == TASK_TERMINATED) {
        return;
    }

    uint8_t prio = p->sched.priority;
    if (prio >= MAX_PRIORITY) {
        prio = MAX_PRIORITY - 1;
    }

    p->sched.priority = prio;
    list_push_ready(&ready_list[prio], p);
    top_ready_priority_bitmap |= (1UL << prio);
    p->state = TASK_READY;
}

void remove_task_from_ready_queue(TCB_t *p)
{
    if (p == NULL) {
        return;
    }

    uint8_t prio = p->sched.priority;
    if (prio >= MAX_PRIORITY) {
        prio = MAX_PRIORITY - 1;
    }

    list_remove_ready(&ready_list[prio], p);

    if (list_is_empty(&ready_list[prio])) {
        top_ready_priority_bitmap &= ~(1UL << prio);
    }
}

static uint8_t highest_ready_priority(void)
{
    int highest_prio = 31 - __builtin_clz(top_ready_priority_bitmap);
    if (highest_prio >= MAX_PRIORITY) {
        highest_prio = MAX_PRIORITY - 1;
    }

    return (uint8_t)highest_prio;
}

TCB_t *get_highest_priority_ready_task(void)
{
    if (top_ready_priority_bitmap == 0) {
        return NULL;
    }

    uint8_t highest_prio = highest_ready_priority();

    list_node_t *node = list_pop_front(&ready_list[highest_prio]);
    TCB_t *p = node ? list_entry(node, TCB_t, node) : NULL;

    if (list_is_empty(&ready_list[highest_prio])) {
        top_ready_priority_bitmap &= ~(1UL << highest_prio);
    }

    return p;
}

void os_schedule(void)
{
    uint32_t irq_state = os_enter_critical();

    if (scheduler_lock_count > 0) {
        scheduler_yield_pending = 1;
        os_exit_critical(irq_state);
        return;
    }

    if (current_tcb != NULL) {
        task_check_stack(current_tcb);
    }

    if (current_tcb != NULL && current_tcb->state == TASK_RUNNING) {
        add_task_to_ready_queue(current_tcb);
    }

    TCB_t *pnext = get_highest_priority_ready_task();
    if (pnext == NULL) {
        if (current_tcb != NULL && current_tcb->state == TASK_RUNNING) {
            current_tcb->state = TASK_RUNNING;
        }
        os_exit_critical(irq_state);
        return;
    }

    pnext->state = TASK_RUNNING;
    pnext->sched.remaining_ticks = OS_TIME_SLICE_TICKS;
    next_tcb = pnext;

    if (current_tcb != next_tcb) {
        port_yield();
    }

    os_exit_critical(irq_state);
}

void scheduler_yield_if_needed(void)
{
#if OS_USE_PREEMPTION
    if (current_tcb == NULL || top_ready_priority_bitmap == 0) {
        return;
    }

    uint8_t highest_prio = highest_ready_priority();
    if (highest_prio > current_tcb->sched.priority) {
        if (scheduler_lock_count > 0) {
            scheduler_yield_pending = 1;
        } else {
            os_schedule();
        }
    }
#endif
}

void scheduler_tick(void)
{
#if OS_USE_TIME_SLICING
    if (current_tcb == NULL || current_tcb->state != TASK_RUNNING) {
        return;
    }

    task_check_stack(current_tcb);

    if ((top_ready_priority_bitmap & (1UL << current_tcb->sched.priority)) == 0) {
        return;
    }

    if (current_tcb->sched.remaining_ticks > 0) {
        current_tcb->sched.remaining_ticks--;
    }

    if (current_tcb->sched.remaining_ticks == 0) {
        os_schedule();
    }
#endif
}

void os_start(void)
{
    current_tcb = get_highest_priority_ready_task();

    if (current_tcb != NULL) {
        current_tcb->state = TASK_RUNNING;
        current_tcb->sched.remaining_ticks = OS_TIME_SLICE_TICKS;
        next_tcb = current_tcb;

        port_start_scheduler(current_tcb->stack_ptr);
    }
}
