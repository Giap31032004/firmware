#include "app_global.h"
#include "heap.h"
#include "kernel_config.h"
#include "os_trace.h"
#include "runtime_stats.h"
#include "task.h"
#include "uart.h"

static volatile uint8_t monitor_enabled = APP_MONITOR_DEFAULT_ENABLED;

void app_monitor_set_enabled(int enabled)
{
    monitor_enabled = enabled != 0 ? 1U : 0U;
}

int app_monitor_is_enabled(void)
{
    return monitor_enabled != 0U;
}

void app_monitor_print_once(void)
{
    os_heap_stats_t heap;
    app_thermal_snapshot_t thermal;
    uint32_t active_tasks = 0U;

    os_get_heap_stats(&heap);
    (void)app_get_thermal_snapshot(&thermal, 20U);

    for (uint32_t tid = 0; tid < MAX_TASKS; tid++) {
        if (tcb_table[tid].state != TASK_UNUSED &&
            tcb_table[tid].state != TASK_TERMINATED) {
            active_tasks++;
        }
    }

    uart_print("\r\n=============== MyOS MONITOR ===============\r\n");
    uart_print("Tick=");
    uart_print_dec(os_tick_count);
    uart_print("  Tasks=");
    uart_print_dec(active_tasks);
    uart_print("/");
    uart_print_dec(MAX_TASKS);
    uart_print("  Tickless=");
    uart_print(OS_USE_TICKLESS_IDLE ? "ON" : "OFF");
    uart_print("  Trace=");
    uart_print(OS_USE_TRACE_FACILITY ? "ON" : "OFF");
    uart_print("\r\nHeap free=");
    uart_print_dec((uint32_t)heap.free_bytes);
    uart_print("  Min=");
    uart_print_dec((uint32_t)heap.minimum_ever_free_bytes);
    uart_print("  Frag=");
    uart_print_dec(heap.fragmentation_percent);
    uart_print("%\r\nQueue=");
    uart_print_dec((uint32_t)sem_get_count(&temp_queue.sem_data));
    uart_print("/");
    uart_print_dec(temp_queue.length);
    uart_print("  Temp=");
    uart_print_dec((uint32_t)thermal.filtered_temperature);
    uart_print("C  Zone=");
    uart_print(app_temp_zone_str(thermal.zone));
    uart_print("  Fan=");
    uart_print_dec((uint32_t)thermal.fan_pwm_percent);
    uart_print("%\r\n");

#if APP_MONITOR_PRINT_STATS
    runtime_stats_print();
#endif
    uart_print("Trace: use command 'trace' for recent events\r\n");
    uart_print("============================================\r\n");
}

void task_runtime_monitor(void)
{
    uart_print("[MONITOR] ready; use 'monitor on' to enable periodic summary\r\n");

    while (1) {
        task_delay(APP_MONITOR_PERIOD_TICKS);

        if (monitor_enabled != 0U) {
            app_monitor_print_once();
            uart_print("\r\nMyOS> ");
        }
    }
}
