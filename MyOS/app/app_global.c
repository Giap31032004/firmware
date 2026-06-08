#include "app_global.h"
#include "heap.h"
#include "kernel.h"
#include "kernel_config.h"
#include "low_power.h"
#include "rtos_tests.h"
#include "task.h"
#include "uart.h"
#include "utils.h"

static app_thermal_snapshot_t thermal_state = {
    .raw_temperature = 25,
    .filtered_temperature = 25,
    .min_temperature = 25,
    .max_temperature = 25,
    .fan_pwm_percent = 0,
    .zone = TEMP_ZONE_NORMAL
};

os_msg_queue_t temp_queue;
os_mutex_t app_mutex;

void task_sensor(void);
void task_controller(void);
void task_shell(void);
void task_gpio_blink(void);

#ifndef MYOS_TEST_SCENARIO
static void app_create_task_or_log(void (*func)(void),
                                   uint8_t priority,
                                   uint32_t stack_words,
                                   const char *name)
{
    uint32_t tid = TASK_INVALID_TID;
    os_status_t status = task_create_dynamic(func, priority, stack_words, &tid);

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

void app_record_sample(int raw, int filtered)
{
    if (mutex_lock_timeout(&app_mutex, 5) != OS_OK) {
        return;
    }

    thermal_state.raw_temperature = raw;
    thermal_state.filtered_temperature = filtered;
    if (raw < thermal_state.min_temperature) {
        thermal_state.min_temperature = raw;
    }
    if (raw > thermal_state.max_temperature) {
        thermal_state.max_temperature = raw;
    }
    thermal_state.uptime_ticks += APP_TEMP_SAMPLE_PERIOD;
    thermal_state.samples++;

    mutex_unlock(&app_mutex);
}

void app_record_control(temp_zone_t zone, int fan_pwm_percent)
{
    if (mutex_lock_timeout(&app_mutex, 5) != OS_OK) {
        return;
    }

    if (zone != TEMP_ZONE_NORMAL && zone != thermal_state.zone) {
        thermal_state.alarm_events++;
    }
    thermal_state.zone = zone;
    thermal_state.fan_pwm_percent = fan_pwm_percent;

    mutex_unlock(&app_mutex);
}

os_status_t app_get_thermal_snapshot(app_thermal_snapshot_t *snapshot,
                                     uint32_t timeout_ticks)
{
    if (snapshot == NULL) {
        return OS_ERROR;
    }

    if (mutex_lock_timeout(&app_mutex, timeout_ticks) != OS_OK) {
        return OS_TIMEOUT;
    }

    *snapshot = thermal_state;
    mutex_unlock(&app_mutex);
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
        uart_print(info.stack_ok != 0U ? "OK" : "BAD");
        uart_print("     ");
        uart_print_dec(info.stack_free_words);
        uart_print("\r\n");
    }

    mutex_unlock(&app_mutex);
}

void app_print_system_status(void)
{
    app_thermal_snapshot_t snapshot;

    if (app_get_thermal_snapshot(&snapshot, 20) != OS_OK ||
        mutex_lock_timeout(&app_mutex, 20) != OS_OK) {
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
    app_print_u32("samples: ", snapshot.samples);
    app_print_u32("alarm events: ", snapshot.alarm_events);

    mutex_unlock(&app_mutex);
}

void app_print_queue_status(void)
{
    if (mutex_lock_timeout(&app_mutex, 20) != OS_OK) {
        return;
    }

    uart_print("\r\n[TEMPERATURE QUEUE]\r\n");
    app_print_u32("used: ", (uint32_t)sem_get_count(&temp_queue.sem_data));
    app_print_u32("free: ", (uint32_t)sem_get_count(&temp_queue.sem_space));
    app_print_u32("length: ", temp_queue.length);

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

void app_print_demo_summary(void)
{
    app_print_line("[DEMO] sensor/filter -> queue -> controller/alarm/log");
    app_print_line("[DEMO] tasks: sensor, controller, shell, gpio_blink");
}

void service_init(void)
{
    if (msg_queue_init(&temp_queue, MAX_MESSAGE_COUNT, sizeof(int32_t)) != OS_OK) {
        kernel_panic("temperature queue init failed", __FILE__, __LINE__);
    }

    mutex_init(&app_mutex);
}

void app_init(void)
{
#ifdef MYOS_TEST_SCENARIO
    rtos_test_init();
#else
    app_print_line("[APP] Starting compact thermal controller");

    app_create_task_or_log(task_sensor, PRIORITY_HIGH,
                           OS_DEFAULT_STACK_WORDS, "sensor");
    app_create_task_or_log(task_controller, PRIORITY_HIGH,
                           OS_DEFAULT_STACK_WORDS, "controller");
    app_create_task_or_log(task_shell, PRIORITY_NORMAL,
                           OS_DEFAULT_STACK_WORDS * 2U, "shell");
    app_create_task_or_log(task_gpio_blink, PRIORITY_LOW,
                           OS_MIN_STACK_WORDS, "gpio_blink");
#endif
}
