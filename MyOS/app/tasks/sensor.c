#include "app_global.h"
#include "kernel.h"
#include "uart.h"

static int virtual_sensor_read_temperature(void)
{
    static int temp = 24;
    static int direction = 1;
    static int noise = -2;

    temp += direction * 2;
    noise++;
    if (noise > 2) {
        noise = -2;
    }

    if (temp >= 58) {
        direction = -1;
    } else if (temp <= 22) {
        direction = 1;
    }

    return temp + noise;
}

void task_temp_sampler(void)
{
    while (1) {
        os_delay(APP_TEMP_SAMPLE_PERIOD);

        int raw = virtual_sensor_read_temperature();

        if (mutex_lock_timeout(&temp_state_mutex, 5) == OS_OK) {
            current_temperature = raw;
            if (raw < min_temperature) {
                min_temperature = raw;
            }
            if (raw > max_temperature) {
                max_temperature = raw;
            }
            system_uptime += APP_TEMP_SAMPLE_PERIOD;
            sensor_samples++;
            mutex_unlock(&temp_state_mutex);
        }

        msg_queue_send(&temp_raw_queue, &raw);
        event_group_set_bits(&app_events,
                             APP_EVENT_SENSOR_SAMPLE |
                             APP_EVENT_HEARTBEAT);
        sem_signal(&heartbeat_sem);
    }
}

void task_temp_filter(void)
{
    int window[4] = {25, 25, 25, 25};
    int index = 0;
    int sum = 100;

    while (1) {
        int raw = 0;
        msg_queue_receive(&temp_raw_queue, &raw);

        sum -= window[index];
        window[index] = raw;
        sum += raw;
        index = (index + 1) & 0x3;

        int filtered = sum / 4;

        if (mutex_lock_timeout(&temp_state_mutex, 5) == OS_OK) {
            filtered_temperature = filtered;
            filter_samples++;
            mutex_unlock(&temp_state_mutex);
        }

        msg_queue_send(&temp_filtered_queue, &filtered);
        msg_queue_send(&temp_display_queue, &filtered);
        msg_queue_send(&temp_log_queue, &filtered);
        event_group_set_bits(&app_events, APP_EVENT_FILTER_UPDATE);
    }
}

void task_temp_controller(void)
{
    while (1) {
        int filtered = 0;
        temp_zone_t next_zone = TEMP_ZONE_NORMAL;
        int next_fan = 0;

        msg_queue_receive(&temp_filtered_queue, &filtered);

        if (filtered >= APP_TEMP_CRITICAL_THRESHOLD) {
            next_zone = TEMP_ZONE_CRITICAL;
            next_fan = 100;
        } else if (filtered >= APP_TEMP_WARN_THRESHOLD) {
            next_zone = TEMP_ZONE_WARN;
            next_fan = 60;
        }

        if (mutex_lock_timeout(&temp_state_mutex, 5) == OS_OK) {
            temp_zone = next_zone;
            fan_pwm_percent = next_fan;
            control_cycles++;
            mutex_unlock(&temp_state_mutex);
        }

        if (next_zone != TEMP_ZONE_NORMAL) {
            event_group_set_bits(&app_events, APP_EVENT_ALARM_ACTIVE);
            sem_signal(&alarm_sem);
        } else {
            event_group_clear_bits(&app_events, APP_EVENT_ALARM_ACTIVE);
        }

        event_group_set_bits(&app_events, APP_EVENT_CONTROL_UPDATE);
    }
}

void task_temp_display(void)
{
    uint32_t frame = 0;
    uint32_t last_print_tick = 0;

    while (1) {
        int filtered = 0;
        app_thermal_snapshot_t snapshot;

        msg_queue_receive(&temp_display_queue, &filtered);

        if (app_get_thermal_snapshot(&snapshot, 5) != OS_OK) {
            continue;
        }

        if ((uint32_t)(os_tick_count - last_print_tick) < APP_DISPLAY_PERIOD_TICKS) {
            continue;
        }
        last_print_tick = os_tick_count;

        if (mutex_lock_timeout(&app_mutex, 20) == OS_OK) {
            uart_print("\r\n+---------------- THERMAL PANEL ----------------+\r\n");
            uart_print("| frame=");
            uart_print_dec(frame++);
            uart_print(" raw=");
            uart_print_dec((uint32_t)snapshot.raw_temperature);
            uart_print("C filtered=");
            uart_print_dec((uint32_t)filtered);
            uart_print("C |\r\n");
            uart_print("| zone=");
            uart_print(app_temp_zone_str(snapshot.zone));
            uart_print(" fan=");
            uart_print_dec((uint32_t)snapshot.fan_pwm_percent);
            uart_print("%\r\n");
            uart_print("+-----------------------------------------------+\r\n");
            display_updates++;
            mutex_unlock(&app_mutex);
        }
    }
}

void task_temp_alarm(void)
{
    temp_zone_t last_zone = TEMP_ZONE_NORMAL;

    while (1) {
        os_status_t result = sem_wait_timeout(&alarm_sem, 150);
        app_thermal_snapshot_t snapshot;

        if (app_get_thermal_snapshot(&snapshot, 5) != OS_OK) {
            continue;
        }

        if (result == OS_TIMEOUT &&
            snapshot.zone == TEMP_ZONE_NORMAL &&
            last_zone != TEMP_ZONE_NORMAL) {
            app_print_line("[ALARM] Thermal state returned to normal.");
            last_zone = TEMP_ZONE_NORMAL;
            continue;
        }

        if (snapshot.zone != TEMP_ZONE_NORMAL && snapshot.zone != last_zone) {
            if (mutex_lock_timeout(&app_mutex, 20) == OS_OK) {
                uart_print("\r\n!!! THERMAL ");
                uart_print(app_temp_zone_str(snapshot.zone));
                uart_print(" alarm, filtered=");
                uart_print_dec((uint32_t)snapshot.filtered_temperature);
                uart_print("C !!!\r\n");
                alarm_events++;
                mutex_unlock(&app_mutex);
            }
            last_zone = snapshot.zone;
        }
    }
}
