#include <stdint.h>

#include "critical.h"
#include "hardware_config.h"
#include "kernel.h"
#include "kernel_config.h"
#include "low_power.h"
#include "os_trace.h"
#include "scheduler.h"
#include "systick.h"
#include "task.h"

static uint32_t low_power_next_timeout_ticks(void)
{
    uint32_t now = os_tick_count;
    uint32_t best_delta = UINT32_MAX;

    for (uint32_t i = 0; i < MAX_TASKS; i++) {
        TCB_t *task = &tcb_table[i];

        if (task->state == TASK_UNUSED ||
            task->state == TASK_TERMINATED ||
            task->wait_has_timeout == 0U) {
            continue;
        }

        if (tick_after_or_equal(now, task->time.wakeup_tick)) {
            return 0U;
        }

        uint32_t delta = task->time.wakeup_tick - now;
        if (delta < best_delta) {
            best_delta = delta;
        }
    }

    return best_delta;
}

__attribute__((weak))
int os_low_power_can_sleep(void)
{
    return 1;
}

void os_low_power_try_sleep(void)
{
#if defined(OS_USE_TICKLESS_IDLE) && OS_USE_TICKLESS_IDLE == 1
    uint32_t expected_idle_ticks;
    uint32_t programmed_ticks;
    uint32_t slept_ticks;
    uint32_t irq_state;

    if (current_tcb == NULL || current_tcb->entry != prvIdleTask) {
        return;
    }

    if (top_ready_priority_bitmap != 0U) {
        return;
    }

    if (!os_low_power_can_sleep()) {
        return;
    }

    expected_idle_ticks = low_power_next_timeout_ticks();
    if (expected_idle_ticks != UINT32_MAX &&
        expected_idle_ticks < OS_EXPECTED_IDLE_TIME_BEFORE_SLEEP) {
        return;
    }

    irq_state = os_enter_critical();

    if (top_ready_priority_bitmap != 0U || !os_low_power_can_sleep()) {
        os_exit_critical(irq_state);
        return;
    }

    expected_idle_ticks = low_power_next_timeout_ticks();
    if (expected_idle_ticks != UINT32_MAX &&
        expected_idle_ticks < OS_EXPECTED_IDLE_TIME_BEFORE_SLEEP) {
        os_exit_critical(irq_state);
        return;
    }

    if (expected_idle_ticks == UINT32_MAX) {
        MYOS_TRACE(OS_TRACE_LOW_POWER_BEGIN, current_tcb->tid, expected_idle_ticks, 0U);
        systick_disable();
        __asm volatile ("dsb" ::: "memory");
        __asm volatile ("wfi" ::: "memory");
        __asm volatile ("isb" ::: "memory");
        (void)systick_init(SYSTICK_RATE_HZ);
        MYOS_TRACE(OS_TRACE_LOW_POWER_END, current_tcb->tid, UINT32_MAX, 0U);
        os_exit_critical(irq_state);
        return;
    }

    programmed_ticks = systick_start_oneshot(expected_idle_ticks);
    if (programmed_ticks == 0U) {
        (void)systick_init(SYSTICK_RATE_HZ);
        os_exit_critical(irq_state);
        return;
    }

    MYOS_TRACE(OS_TRACE_LOW_POWER_BEGIN, current_tcb->tid, programmed_ticks, 0U);
    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("wfi" ::: "memory");
    __asm volatile ("isb" ::: "memory");

    slept_ticks = systick_elapsed_ticks(programmed_ticks);
    systick_disable();
    systick_clear_pending();
    timer_step_ticks(slept_ticks);
    (void)systick_init(SYSTICK_RATE_HZ);
    MYOS_TRACE(OS_TRACE_LOW_POWER_END, current_tcb->tid, slept_ticks, programmed_ticks);

    os_exit_critical(irq_state);
#endif
}
