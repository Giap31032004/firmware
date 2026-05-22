#include "kernel.h"
#include "critical.h"
#include "scheduler.h"
#include "task.h"

volatile uint32_t os_tick_count = 0;
static list_t delay_list;

void timer_init(void)
{
    os_tick_count = 0;
    list_init(&delay_list);
}

void os_delay(uint32_t ticks)
{
    if (ticks == 0) {
        return;
    }

    (void)task_block_current_on(&delay_list, TASK_WAITING_TIME, ticks);
}

void process_timer_tick(void)
{
    os_tick_count++;

    task_process_timeouts();
    scheduler_yield_if_needed();
    scheduler_tick();
}
