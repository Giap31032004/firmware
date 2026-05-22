#include "app_global.h"
#include "kernel.h"
#include "scheduler.h"
#include "uart.h"

void task_sensor_update(void)
{
    int local_temp = 25;
    int direction = 1;

    while (1) {
        os_delay(25);

        if (direction > 0) {
            local_temp += 5;
            if (local_temp >= 55) {
                direction = -1;
            }
        } else {
            local_temp -= 5;
            if (local_temp <= 20) {
                direction = 1;
            }
        }

        scheduler_lock();
        current_temperature = local_temp;
        system_uptime += 25;
        sensor_samples++;
        scheduler_unlock();

        msg_queue_send(&temp_queue, local_temp);

        if (local_temp > APP_TEMP_ALARM_THRESHOLD) {
            sem_signal(&alarm_sem);
        }

        sem_signal(&heartbeat_sem);
    }
}

void task_display(void)
{
    while (1) {
        int received_temp = msg_queue_receive(&temp_queue);

        if (mutex_lock_timeout(&app_mutex, 20) == OS_OK) {
            uart_print("----------------------\r\n");
            uart_print("| Temp: ");
            uart_print_dec((uint32_t)received_temp);
            uart_print(" C         |\r\n");
            uart_print("----------------------\r\n");
            display_updates++;
            mutex_unlock(&app_mutex);
        }
    }
}

void task_alarm(void)
{
    int alarm_active = 0;

    while (1) {
        os_status_t wait_result = sem_wait_timeout(&alarm_sem, 100);

        if (wait_result == OS_TIMEOUT && current_temperature <= APP_TEMP_ALARM_THRESHOLD) {
            if (alarm_active) {
                app_print_line("[ALARM] Temperature normal.");
                alarm_active = 0;
            }
            continue;
        }

        if (current_temperature > APP_TEMP_ALARM_THRESHOLD && !alarm_active) {
            if (mutex_lock_timeout(&app_mutex, 20) == OS_OK) {
                uart_print("\r\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
                uart_print("!!! [ALARM] WARNING: OVERHEAT !!!\r\n");
                uart_print("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
                alarm_events++;
                mutex_unlock(&app_mutex);
            }
            alarm_active = 1;
        }
    }
}
