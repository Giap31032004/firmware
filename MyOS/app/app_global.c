#include "app_global.h"
#include "heap.h"
#include "kernel.h"
#include "kernel_config.h"
#include "low_power.h"
#include "uart.h"
#include "utils.h"
#include "rtos_tests.h"

volatile int current_temperature = 25;
volatile int filtered_temperature = 25;
volatile int min_temperature = 25;
volatile int max_temperature = 25;
volatile int fan_pwm_percent = 0;
volatile temp_zone_t temp_zone = TEMP_ZONE_NORMAL;
volatile int system_uptime = 0;
volatile uint32_t sensor_samples = 0;
volatile uint32_t filter_samples = 0;
volatile uint32_t display_updates = 0;
volatile uint32_t alarm_events = 0;
volatile uint32_t log_records = 0;
volatile uint32_t storage_cycles = 0;
volatile uint32_t control_cycles = 0;

os_msg_queue_t temp_raw_queue;
os_msg_queue_t temp_filtered_queue;
os_msg_queue_t temp_display_queue;
os_msg_queue_t temp_log_queue;

os_mutex_t app_mutex;
os_mutex_t temp_state_mutex;
os_mutex_t storage_mutex;
os_sem_t alarm_sem;
os_sem_t heartbeat_sem;
os_event_group_t app_events;

void task_temp_sampler(void);
void task_temp_filter(void);
void task_temp_controller(void);
void task_temp_display(void);
void task_temp_alarm(void);
void task_temp_logger(void);
void task_temp_storage(void);
void task_health_monitor(void);
void task_shell(void);
void task_gpio_blink(void);

#ifndef MYOS_TEST_SCENARIO
static void app_create_task_or_log(void (*func)(void),
                                   uint8_t priority,
                                   uint32_t stack_words,
                                   const char *name)
{
    uint32_t tid = TASK_INVALID_TID;
    os_status_t status = task_create_with_stack(func, priority, stack_words, &tid);

    if (status != OS_OK) {
        uart_print("[APP] Failed to create task: ");
        uart_print(name);
        uart_print("\r\n");
    }
}
#endif

const char *app_temp_zone_str(temp_zone_t zone)
{
    switch (zone) {
    case TEMP_ZONE_NORMAL:
        return "NORMAL";
    case TEMP_ZONE_WARN:
        return "WARN";
    case TEMP_ZONE_CRITICAL:
        return "CRITICAL";
    default:
        return "UNKNOWN";
    }
}

static void app_print_u32(const char *label, uint32_t value)
{
    uart_print(label);
    uart_print_dec(value);
    uart_print("\r\n");
}

void app_print_line(const char *s)
{
    if (mutex_lock_timeout(&app_mutex, 20) == OS_OK) {
        uart_print(s);
        uart_print("\r\n");
        mutex_unlock(&app_mutex);
    }
}

os_status_t app_get_thermal_snapshot(app_thermal_snapshot_t *snapshot,
                                     uint32_t timeout_ticks)
{
    if (snapshot == NULL) {
        return OS_ERROR;
    }

    if (mutex_lock_timeout(&temp_state_mutex, timeout_ticks) != OS_OK) {
        return OS_TIMEOUT;
    }

    snapshot->raw_temperature = current_temperature;
    snapshot->filtered_temperature = filtered_temperature;
    snapshot->min_temperature = min_temperature;
    snapshot->max_temperature = max_temperature;
    snapshot->fan_pwm_percent = fan_pwm_percent;
    snapshot->zone = temp_zone;
    snapshot->uptime_ticks = (uint32_t)system_uptime;
    snapshot->sensor_samples = sensor_samples;
    snapshot->filter_samples = filter_samples;
    snapshot->display_updates = display_updates;
    snapshot->alarm_events = alarm_events;
    snapshot->log_records = log_records;
    snapshot->storage_cycles = storage_cycles;
    snapshot->control_cycles = control_cycles;

    mutex_unlock(&temp_state_mutex);
    return OS_OK;
}

void app_print_task_table(void)
{
    os_task_info_t info;

    if (mutex_lock_timeout(&app_mutex, 20) != OS_OK) {
        return;
    }

    uart_print("\r\nTID  STATE       PRIO  STACK  FREE_WORDS\r\n");
    uart_print("-------------------------------------------\r\n");

    for (uint32_t i = 0; i < MAX_TASKS; i++) {
        if (os_task_get_info(i, &info) != OS_OK) {
            continue;
        }

        uart_print_dec(info.tid);
        uart_print("    ");
        uart_print(task_state_str(info.state));
        uart_print("    ");
        uart_print_dec(info.priority);
        uart_print("     ");
        if (info.stack_ok != 0U) {
            uart_print("OK");
        } else {
            uart_print("BAD");
        }
        uart_print("     ");
        uart_print_dec(info.stack_free_words);
        uart_print("\r\n");
    }

    mutex_unlock(&app_mutex);
}

void app_print_system_status(void)
{
    app_thermal_snapshot_t snapshot;

    if (app_get_thermal_snapshot(&snapshot, 20) != OS_OK) {
        return;
    }

    if (mutex_lock_timeout(&app_mutex, 20) != OS_OK) {
        return;
    }

    uart_print("\r\n[THERMAL STATUS]\r\n");
    app_print_u32("uptime ticks: ", snapshot.uptime_ticks);
    app_print_u32("raw temp: ", (uint32_t)snapshot.raw_temperature);
    app_print_u32("filtered temp: ", (uint32_t)snapshot.filtered_temperature);
    app_print_u32("min temp: ", (uint32_t)snapshot.min_temperature);
    app_print_u32("max temp: ", (uint32_t)snapshot.max_temperature);
    app_print_u32("fan pwm: ", (uint32_t)snapshot.fan_pwm_percent);
    uart_print("zone: ");
    uart_print(app_temp_zone_str(snapshot.zone));
    uart_print("\r\n");
    app_print_u32("samples: ", snapshot.sensor_samples);
    app_print_u32("filtered samples: ", snapshot.filter_samples);
    app_print_u32("display updates: ", snapshot.display_updates);
    app_print_u32("control cycles: ", snapshot.control_cycles);
    app_print_u32("alarm events: ", snapshot.alarm_events);
    app_print_u32("log records: ", snapshot.log_records);
    app_print_u32("storage cycles: ", snapshot.storage_cycles);

    mutex_unlock(&app_mutex);
}

static void app_print_queue_line(const char *name, os_msg_queue_t *queue)
{
    uart_print(name);
    uart_print(" used=");
    uart_print_dec((uint32_t)sem_get_count(&queue->sem_data));
    uart_print(" free=");
    uart_print_dec((uint32_t)sem_get_count(&queue->sem_space));
    uart_print(" len=");
    uart_print_dec(queue->length);
    uart_print("\r\n");
}

void app_print_queue_status(void)
{
    if (mutex_lock_timeout(&app_mutex, 20) != OS_OK) {
        return;
    }

    uart_print("\r\n[QUEUE STATUS]\r\n");
    app_print_queue_line("raw     ", &temp_raw_queue);
    app_print_queue_line("filtered", &temp_filtered_queue);
    app_print_queue_line("display ", &temp_display_queue);
    app_print_queue_line("log     ", &temp_log_queue);

    mutex_unlock(&app_mutex);
}

void app_print_heap_status(void)
{
    os_heap_stats_t stats;
    os_get_heap_stats(&stats);

    if (mutex_lock_timeout(&app_mutex, 20) != OS_OK) {
        return;
    }

    uart_print("\r\n[HEAP STATUS]\r\n");
    app_print_u32("free bytes: ", (uint32_t)stats.free_bytes);
    app_print_u32("min ever free: ", (uint32_t)stats.minimum_ever_free_bytes);
    app_print_u32("largest block: ", (uint32_t)stats.largest_free_block);
    app_print_u32("free blocks: ", (uint32_t)stats.free_block_count);
    app_print_u32("fragmentation %: ", stats.fragmentation_percent);

    mutex_unlock(&app_mutex);
}

void app_print_power_status(void)
{
    if (mutex_lock_timeout(&app_mutex, 20) != OS_OK) {
        return;
    }

    uart_print("\r\n[POWER STATUS]\r\n");
    app_print_u32("tickless enabled: ", OS_USE_TICKLESS_IDLE);
    app_print_u32("can sleep now: ", (uint32_t)os_low_power_can_sleep());
    app_print_u32("idle threshold ticks: ", OS_EXPECTED_IDLE_TIME_BEFORE_SLEEP);

    mutex_unlock(&app_mutex);
}

void app_print_event_status(void)
{
    os_event_bits_t bits = event_group_get_bits(&app_events);

    if (mutex_lock_timeout(&app_mutex, 20) != OS_OK) {
        return;
    }

    uart_print("\r\n[APP EVENTS]\r\n");
    app_print_u32("bits: ", bits);
    app_print_u32("sensor sample: ", (bits & APP_EVENT_SENSOR_SAMPLE) != 0U);
    app_print_u32("filter update: ", (bits & APP_EVENT_FILTER_UPDATE) != 0U);
    app_print_u32("control update: ", (bits & APP_EVENT_CONTROL_UPDATE) != 0U);
    app_print_u32("alarm active: ", (bits & APP_EVENT_ALARM_ACTIVE) != 0U);
    app_print_u32("heartbeat: ", (bits & APP_EVENT_HEARTBEAT) != 0U);

    mutex_unlock(&app_mutex);
}

void app_print_demo_summary(void)
{
    if (mutex_lock_timeout(&app_mutex, 20) != OS_OK) {
        return;
    }

    uart_print("\r\n[DEMO]\r\n");
    uart_print("thermal-control pipeline: sampler -> filter -> controller -> display/log\r\n");
    uart_print("RTOS features: preemptive tasks, priorities, queues, mutex, semaphores, event group, shell, heap stats, tickless idle\r\n");
    uart_print("Use: ps, thermal, queues, events, heap, power, stats, trace\r\n");

    mutex_unlock(&app_mutex);
}

void service_init(void)
{
    if (msg_queue_init(&temp_raw_queue, MAX_MESSAGE_COUNT, sizeof(int32_t)) != OS_OK ||
        msg_queue_init(&temp_filtered_queue, MAX_MESSAGE_COUNT, sizeof(int32_t)) != OS_OK ||
        msg_queue_init(&temp_display_queue, MAX_MESSAGE_COUNT, sizeof(int32_t)) != OS_OK ||
        msg_queue_init(&temp_log_queue, MAX_MESSAGE_COUNT, sizeof(int32_t)) != OS_OK) {
        kernel_panic("app queue init failed", __FILE__, __LINE__);
    }

    mutex_init(&app_mutex);
    mutex_init(&temp_state_mutex);
    mutex_init(&storage_mutex);

    binary_sem_init(&alarm_sem, 0);
    binary_sem_init(&heartbeat_sem, 0);
    event_group_init(&app_events);
}

void app_init(void)
{
#ifdef MYOS_TEST_SCENARIO
    rtos_test_init();
#else
    app_print_line("[APP] Starting thermal control pipeline");

    app_create_task_or_log(task_temp_sampler, PRIORITY_HIGH,
                           OS_DEFAULT_STACK_WORDS, "temp_sampler");
    app_create_task_or_log(task_temp_filter, PRIORITY_NORMAL,
                           OS_DEFAULT_STACK_WORDS, "temp_filter");
    app_create_task_or_log(task_temp_controller, PRIORITY_HIGH,
                           OS_DEFAULT_STACK_WORDS, "temp_controller");
    app_create_task_or_log(task_temp_display, PRIORITY_NORMAL,
                           OS_DEFAULT_STACK_WORDS, "temp_display");
    app_create_task_or_log(task_temp_alarm, PRIORITY_REALTIME,
                           OS_DEFAULT_STACK_WORDS, "temp_alarm");
    app_create_task_or_log(task_temp_logger, PRIORITY_LOW,
                           OS_DEFAULT_STACK_WORDS, "temp_logger");
    app_create_task_or_log(task_temp_storage, PRIORITY_LOW,
                           OS_DEFAULT_STACK_WORDS, "temp_storage");
    app_create_task_or_log(task_health_monitor, PRIORITY_LOW,
                           OS_DEFAULT_STACK_WORDS, "health_monitor");
    app_create_task_or_log(task_shell, PRIORITY_NORMAL,
                           OS_DEFAULT_STACK_WORDS * 2U, "shell");
    app_create_task_or_log(task_gpio_blink, PRIORITY_LOW,
                           OS_MIN_STACK_WORDS, "gpio_blink");
#endif
}
