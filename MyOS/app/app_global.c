#include "app_global.h"
#include "kernel.h"
#include "scheduler.h"
#include "uart.h"
#include "utils.h"

volatile int current_temperature = 25;
volatile int system_uptime = 0;
volatile uint32_t sensor_samples = 0;
volatile uint32_t display_updates = 0;
volatile uint32_t alarm_events = 0;
volatile uint32_t resource_cycles = 0;

os_msg_queue_t temp_queue;
os_mutex_t app_mutex;
os_mutex_t mutex_A;
os_mutex_t mutex_B;
os_sem_t alarm_sem;
os_sem_t heartbeat_sem;

void task_sensor_update(void);
void task_display(void);
void task_alarm(void);
void task_logger(void);
void task_shell(void);
void task_gpio_blink(void);
void task_resource_user_a(void);
void task_resource_user_b(void);
void task_health_monitor(void);

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

void app_print_task_table(void)
{
    if (mutex_lock_timeout(&app_mutex, 20) != OS_OK) {
        return;
    }

    uart_print("\r\nTID  STATE       PRIO  STACK\r\n");
    uart_print("--------------------------------\r\n");

    for (uint32_t i = 0; i < MAX_TASKS; i++) {
        if (tcb_table[i].state == TASK_UNUSED) {
            continue;
        }

        uart_print_dec(i);
        uart_print("    ");
        uart_print(task_state_str(tcb_table[i].state));
        uart_print("    ");
        uart_print_dec(tcb_table[i].sched.priority);
        uart_print("     ");
        if (tcb_table[i].mem.stack_base != 0 &&
            *((uint32_t *)tcb_table[i].mem.stack_base) == STACK_CANARY_VALUE) {
            uart_print("OK");
        } else {
            uart_print("BAD");
        }
        uart_print("\r\n");
    }

    mutex_unlock(&app_mutex);
}

void app_print_system_status(void)
{
    if (mutex_lock_timeout(&app_mutex, 20) != OS_OK) {
        return;
    }

    scheduler_lock();
    uint32_t uptime = (uint32_t)system_uptime;
    uint32_t temp = (uint32_t)current_temperature;
    uint32_t samples = sensor_samples;
    uint32_t displays = display_updates;
    uint32_t alarms = alarm_events;
    uint32_t resources = resource_cycles;
    scheduler_unlock();

    uart_print("\r\n[STATUS]\r\n");
    app_print_u32("uptime ticks: ", uptime);
    app_print_u32("temperature: ", temp);
    app_print_u32("sensor samples: ", samples);
    app_print_u32("display updates: ", displays);
    app_print_u32("alarm events: ", alarms);
    app_print_u32("resource cycles: ", resources);

    mutex_unlock(&app_mutex);
}

void service_init(void)
{
    msg_queue_init(&temp_queue);
    mutex_init(&app_mutex);
    mutex_init(&mutex_A);
    mutex_init(&mutex_B);
    sem_init(&alarm_sem, 0);
    sem_init(&heartbeat_sem, 0);
}

void app_init(void)
{
    app_print_line("[APP] Starting OS showcase tasks");

    task_create(task_sensor_update, PRIORITY_NORMAL);
    task_create(task_display, PRIORITY_NORMAL);
    task_create(task_alarm, PRIORITY_HIGH);
    task_create(task_logger, PRIORITY_LOW);
    task_create(task_shell, PRIORITY_NORMAL);
    task_create(task_gpio_blink, PRIORITY_LOW);
    task_create(task_resource_user_a, PRIORITY_LOW);
    task_create(task_resource_user_b, PRIORITY_LOW);
    task_create(task_health_monitor, PRIORITY_LOW);
}
