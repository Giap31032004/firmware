#include <stdint.h>

#include "os_trace.h"
#include "scheduler.h"
#include "task.h"
#include "tick.h"
#include "timer.h"

volatile uint32_t os_tick_count = 0U;

void tick_init(void)
{
    os_tick_count = 0U;
}

void kernel_tick(void)
{
    os_tick_count++;

    timer_process_tick();
    task_process_timeouts();
    scheduler_tick();

    if (current_tcb != NULL && current_tcb->state == TASK_RUNNING) {
        scheduler_yield_if_needed();
    }
}

void tick_step(uint32_t ticks)
{
    if (ticks == 0U) {
        return;
    }

    os_tick_count += ticks;
    MYOS_TRACE(OS_TRACE_TICK_STEP,
               current_tcb != NULL ? current_tcb->tid : UINT32_MAX,
               ticks,
               os_tick_count);

    timer_process_tick();
    task_process_timeouts();

    if (current_tcb != NULL && current_tcb->state == TASK_RUNNING) {
        scheduler_yield_if_needed();
    }
}
