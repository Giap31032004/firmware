#include <stddef.h>
#include <stdint.h>

#include "critical.h"
#include "scheduler.h"
#include "task.h"
#include "port.h"
#include "mpu.h"
#include "os_trace.h"
#include "runtime_stats.h"

list_t ready_list[MAX_PRIORITY];
uint32_t top_ready_priority_bitmap = 0;

TCB_t *current_tcb = NULL;
TCB_t *next_tcb = NULL;

static uint32_t scheduler_lock_count = 0;
static uint8_t scheduler_yield_pending = 0;

void scheduler_init(void)
{
    for (int i = 0; i < MAX_PRIORITY; i++)
    {
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

    if (scheduler_lock_count > 0U)
    {
        scheduler_lock_count--;
    }

    int should_yield = (scheduler_lock_count == 0 && scheduler_yield_pending);
    if (should_yield)
    {
        scheduler_yield_pending = 0;
    }

    os_exit_critical(irq_state);

    if (should_yield)
    {
        os_schedule();
    }
}

void add_task_to_ready_queue(TCB_t *task)
{
    uint32_t irq_state;
    uint8_t priority;

    if (task == NULL)
    {
        return;
    }

    irq_state = os_enter_critical();

    if (task->state == TASK_READY ||
        task->state == TASK_UNUSED ||
        task->state == TASK_TERMINATED)
    {
        os_exit_critical(irq_state);
        return;
    }

    priority = task->sched.priority;
    if (priority >= MAX_PRIORITY)
    {
        priority = MAX_PRIORITY - 1;
    }

    task->sched.priority = priority;
    list_push_back(&ready_list[priority], &task->node);
    top_ready_priority_bitmap |= (1UL << priority);
    task->state = TASK_READY;

    os_exit_critical(irq_state);
}

void remove_task_from_ready_queue(TCB_t *task)
{
    if (task == NULL) {
        return;
    }

    uint32_t irq_state = os_enter_critical();

    if (task->state != TASK_READY ||
        task->sched.priority >= MAX_PRIORITY) {
        os_exit_critical(irq_state);
        return;
    }

    uint8_t priority = task->sched.priority;

    list_remove(&ready_list[priority], &task->node);

    if (list_is_empty(&ready_list[priority])) {
        top_ready_priority_bitmap &= ~(1UL << priority);
    }

    os_exit_critical(irq_state);
}

static uint32_t highest_ready_priority(void)
{
    uint32_t highest_prio =
        31U - (uint32_t)__builtin_clz(top_ready_priority_bitmap);

    if (highest_prio >= MAX_PRIORITY)
    {
        highest_prio = MAX_PRIORITY - 1;
    }

    return highest_prio;
}

TCB_t *get_highest_priority_ready_task(void)
{
    uint32_t irq_state = os_enter_critical();

    if (top_ready_priority_bitmap == 0U) {
        os_exit_critical(irq_state);
        return NULL;
    }

    uint32_t priority = highest_ready_priority();
    list_node_t *node = list_pop_front(&ready_list[priority]);

    if (list_is_empty(&ready_list[priority])) {
        top_ready_priority_bitmap &= ~(1UL << priority);
    }

    TCB_t *task =
        node != NULL ? list_entry(node, TCB_t, node) : NULL;

    os_exit_critical(irq_state);
    return task;
}

void os_schedule(void)
{
    uint32_t irq_state = os_enter_critical();

    if (scheduler_lock_count > 0U)
    {
        scheduler_yield_pending = 1;
        os_exit_critical(irq_state);
        return;
    }

    if (current_tcb != NULL)
    {
        task_check_stack(current_tcb);
    }

    if (current_tcb != NULL && current_tcb->state == TASK_RUNNING)
    {
        add_task_to_ready_queue(current_tcb);
    }

    TCB_t *next_task = get_highest_priority_ready_task();
    if (next_task == NULL)
    {
        os_exit_critical(irq_state);
        return;
    }

    next_task->state = TASK_RUNNING;
    next_task->sched.remaining_ticks = OS_TIME_SLICE_TICKS;
    next_tcb = next_task;

    if (current_tcb != next_tcb)
    {
        if (current_tcb != NULL)
        {
            runtime_stats_task_switched_out(current_tcb);
            MYOS_TRACE(OS_TRACE_TASK_SWITCHED_OUT,
                       current_tcb->tid,
                       current_tcb->state,
                       next_task->tid);
        }

        runtime_stats_task_switched_in(next_tcb);
        MYOS_TRACE(OS_TRACE_TASK_SWITCHED_IN,
                   next_tcb->tid,
                   current_tcb != NULL ? current_tcb->tid : UINT32_MAX,
                   0U);
        mpu_switch_task(&next_tcb->mpu);
        port_yield();
    }

    os_exit_critical(irq_state);
}

void scheduler_yield_if_needed(void)
{
#if OS_USE_PREEMPTION
    if (current_tcb == NULL || top_ready_priority_bitmap == 0U)
    {
        return;
    }

    uint32_t highest_prio = highest_ready_priority();
    if (highest_prio > current_tcb->sched.priority)
    {
        if (scheduler_lock_count > 0U)
        {
            scheduler_yield_pending = 1;
        }
        else
        {
            os_schedule();
        }
    }
#endif
}

void scheduler_tick(void)
{
#if OS_USE_TIME_SLICING
    if (current_tcb == NULL || current_tcb->state != TASK_RUNNING)
    {
        return;
    }

    task_check_stack(current_tcb);

    if ((top_ready_priority_bitmap & (1UL << current_tcb->sched.priority)) == 0)
    {
        return;
    }

    if (current_tcb->sched.remaining_ticks > 0)
    {
        current_tcb->sched.remaining_ticks--;
    }

    if (current_tcb->sched.remaining_ticks == 0)
    {
        os_schedule();
    }
#endif
}

void os_start(void)
{
    current_tcb = get_highest_priority_ready_task();

    if (current_tcb != NULL)
    {
        current_tcb->state = TASK_RUNNING;
        current_tcb->sched.remaining_ticks = OS_TIME_SLICE_TICKS;
        next_tcb = current_tcb;
        runtime_stats_task_switched_in(current_tcb);
        MYOS_TRACE(OS_TRACE_TASK_SWITCHED_IN, current_tcb->tid, UINT32_MAX, 1U);
        mpu_switch_task(&current_tcb->mpu);

        port_start_scheduler(current_tcb->stack_ptr);
    }
}
