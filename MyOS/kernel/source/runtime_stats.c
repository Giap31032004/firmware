#include <stdint.h>

#include "critical.h"
#include "kernel_config.h"
#include "os_log.h"
#include "port.h"
#include "runtime_stats.h"
#include "scheduler.h"
#include "utils.h"

static uint32_t runtime_stats_started;

static void print_percent_x100(uint64_t part, uint64_t total)
{
    uint32_t part32;
    uint32_t total32;
    uint32_t pct_x100;

    if (total == 0U) {
        os_log_write("0.00");
        return;
    }

    while (part > UINT32_MAX || total > UINT32_MAX) {
        part >>= 1U;
        total >>= 1U;
    }

    part32 = (uint32_t)part;
    total32 = (uint32_t)total;
    if (total32 == 0U) {
        os_log_write("0.00");
        return;
    }

    if (total32 >= 10000U) {
        pct_x100 = part32 / (total32 / 10000U);
    } else {
        pct_x100 = (part32 * 10000U) / total32;
    }
    os_log_write_dec(pct_x100 / 100U);
    os_log_write(".");
    if ((pct_x100 % 100U) < 10U) {
        os_log_write("0");
    }
    os_log_write_dec(pct_x100 % 100U);
}

void runtime_stats_init(void)
{
#if defined(OS_GENERATE_RUN_TIME_STATS) && OS_GENERATE_RUN_TIME_STATS == 1
    runtime_stats_started = 1U;
#else
    runtime_stats_started = 0U;
#endif
}

uint32_t runtime_stats_counter(void)
{
    return port_runtime_counter();
}

void runtime_stats_task_switched_in(TCB_t *task)
{
#if defined(OS_GENERATE_RUN_TIME_STATS) && OS_GENERATE_RUN_TIME_STATS == 1
    if (runtime_stats_started != 0U && task != NULL) {
        task->runtime_last_start = runtime_stats_counter();
    }
#else
    (void)task;
#endif
}

void runtime_stats_task_switched_out(TCB_t *task)
{
#if defined(OS_GENERATE_RUN_TIME_STATS) && OS_GENERATE_RUN_TIME_STATS == 1
    uint32_t now;

    if (runtime_stats_started == 0U || task == NULL) {
        return;
    }

    now = runtime_stats_counter();
    task->runtime_cycles += (uint32_t)(now - task->runtime_last_start);
#else
    (void)task;
#endif
}

void runtime_stats_print(void)
{
#if defined(OS_GENERATE_RUN_TIME_STATS) && OS_GENERATE_RUN_TIME_STATS == 1
    uint64_t total = 0U;
    uint32_t irq_state;

    irq_state = os_enter_critical();
    if (current_tcb != NULL && current_tcb->state == TASK_RUNNING) {
        runtime_stats_task_switched_out(current_tcb);
        runtime_stats_task_switched_in(current_tcb);
    }

    for (uint32_t i = 0; i < MAX_TASKS; i++) {
        if (tcb_table[i].state != TASK_UNUSED) {
            total += tcb_table[i].runtime_cycles;
        }
    }
    os_exit_critical(irq_state);

    os_log_write("\r\nID  NAME         STATE      PRIO  CPU%     STACK_FREE\r\n");
    os_log_write("------------------------------------------------------\r\n");

    for (uint32_t i = 0; i < MAX_TASKS; i++) {
        if (tcb_table[i].state == TASK_UNUSED) {
            continue;
        }

        os_log_write_dec(i);
        os_log_write("   ");
        os_log_write_column(tcb_table[i].name != NULL
                                ? tcb_table[i].name
                                : "task",
                            13U);
        os_log_write_column(task_state_str(tcb_table[i].state), 11U);
        os_log_write_dec(tcb_table[i].sched.priority);
        os_log_write("     ");
        print_percent_x100(tcb_table[i].runtime_cycles, total);
        os_log_write("     ");
        os_log_write_dec(task_get_stack_high_water_mark(&tcb_table[i]));
        os_log_write("\r\n");
    }
#else
    os_log_write("Run-time statistics disabled.\r\n");
#endif
}
