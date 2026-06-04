#include <stdint.h>

#include "critical.h"
#include "kernel.h"
#include "kernel_config.h"
#include "os_trace.h"
#include "runtime_stats.h"
#include "scheduler.h"
#include "uart.h"

#if defined(OS_USE_TRACE_FACILITY) && OS_USE_TRACE_FACILITY == 1
static os_trace_event_t trace_buffer[OS_TRACE_BUFFER_SIZE];
static volatile uint32_t trace_write_index;
static volatile uint32_t trace_count;
static volatile uint32_t trace_dropped;

static const char *trace_event_name(uint16_t event)
{
    switch ((os_trace_event_id_t)event) {
    case OS_TRACE_TASK_CREATE: return "task_create";
    case OS_TRACE_TASK_DELETE: return "task_delete";
    case OS_TRACE_TASK_READY: return "task_ready";
    case OS_TRACE_TASK_BLOCK: return "task_block";
    case OS_TRACE_TASK_SUSPEND: return "task_suspend";
    case OS_TRACE_TASK_RESUME: return "task_resume";
    case OS_TRACE_TASK_SWITCHED_OUT: return "task_out";
    case OS_TRACE_TASK_SWITCHED_IN: return "task_in";
    case OS_TRACE_SEM_WAIT: return "sem_wait";
    case OS_TRACE_SEM_SIGNAL: return "sem_signal";
    case OS_TRACE_MUTEX_WAIT: return "mutex_wait";
    case OS_TRACE_MUTEX_LOCK: return "mutex_lock";
    case OS_TRACE_MUTEX_UNLOCK: return "mutex_unlock";
    case OS_TRACE_QUEUE_SEND: return "queue_send";
    case OS_TRACE_QUEUE_RECEIVE: return "queue_recv";
    case OS_TRACE_LOW_POWER_BEGIN: return "sleep_begin";
    case OS_TRACE_LOW_POWER_END: return "sleep_end";
    case OS_TRACE_TICK_STEP: return "tick_step";
    default: return "unknown";
    }
}
#endif

void os_trace_init(void)
{
#if defined(OS_USE_TRACE_FACILITY) && OS_USE_TRACE_FACILITY == 1
    trace_write_index = 0U;
    trace_count = 0U;
    trace_dropped = 0U;
#endif
}

void os_trace_record(os_trace_event_id_t event, uint32_t tid, uint32_t arg0, uint32_t arg1)
{
#if defined(OS_USE_TRACE_FACILITY) && OS_USE_TRACE_FACILITY == 1
    uint32_t irq_state;
    uint32_t index;

    irq_state = os_enter_critical();

    index = trace_write_index;
    trace_buffer[index].time = runtime_stats_counter();
    trace_buffer[index].tick = os_tick_count;
    trace_buffer[index].event = (uint16_t)event;
    trace_buffer[index].tid = (tid > UINT16_MAX) ? UINT16_MAX : (uint16_t)tid;
    trace_buffer[index].arg0 = arg0;
    trace_buffer[index].arg1 = arg1;

    trace_write_index = (trace_write_index + 1U) % OS_TRACE_BUFFER_SIZE;
    if (trace_count < OS_TRACE_BUFFER_SIZE) {
        trace_count++;
    } else {
        trace_dropped++;
    }

    os_exit_critical(irq_state);
#else
    (void)event;
    (void)tid;
    (void)arg0;
    (void)arg1;
#endif
}

void os_trace_clear(void)
{
#if defined(OS_USE_TRACE_FACILITY) && OS_USE_TRACE_FACILITY == 1
    uint32_t irq_state = os_enter_critical();
    trace_write_index = 0U;
    trace_count = 0U;
    trace_dropped = 0U;
    os_exit_critical(irq_state);
#endif
}

void os_trace_dump(uint32_t max_events)
{
#if defined(OS_USE_TRACE_FACILITY) && OS_USE_TRACE_FACILITY == 1
    uint32_t irq_state;
    uint32_t count;
    uint32_t start;
    uint32_t dropped;

    irq_state = os_enter_critical();
    count = trace_count;
    dropped = trace_dropped;
    if (max_events != 0U && count > max_events) {
        count = max_events;
    }
    start = (trace_write_index + OS_TRACE_BUFFER_SIZE - count) % OS_TRACE_BUFFER_SIZE;
    os_exit_critical(irq_state);

    uart_print("\r\nTRACE time tick event tid arg0 arg1\r\n");
    uart_print("-----------------------------------\r\n");
    if (dropped != 0U) {
        uart_print("dropped=");
        uart_print_dec(dropped);
        uart_print("\r\n");
    }

    for (uint32_t i = 0; i < count; i++) {
        os_trace_event_t event;
        uint32_t index = (start + i) % OS_TRACE_BUFFER_SIZE;

        irq_state = os_enter_critical();
        event = trace_buffer[index];
        os_exit_critical(irq_state);

        uart_print_dec(event.time);
        uart_print(" ");
        uart_print_dec(event.tick);
        uart_print(" ");
        uart_print(trace_event_name(event.event));
        uart_print(" ");
        uart_print_dec(event.tid);
        uart_print(" ");
        uart_print_dec(event.arg0);
        uart_print(" ");
        uart_print_dec(event.arg1);
        uart_print("\r\n");
    }
#else
    (void)max_events;
    uart_print("RTOS trace disabled.\r\n");
#endif
}
