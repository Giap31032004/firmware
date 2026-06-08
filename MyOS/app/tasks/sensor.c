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

void task_sensor(void)
{
    int window[4] = {25, 25, 25, 25};
    int index = 0;
    int sum = 100;

    while (1) {
        int raw;
        int filtered;

        os_delay(APP_TEMP_SAMPLE_PERIOD);
        raw = virtual_sensor_read_temperature();

        sum -= window[index];
        window[index] = raw;
        sum += raw;
        index = (index + 1) & 0x3;
        filtered = sum / 4;

        app_record_sample(raw, filtered);
        msg_queue_send(&temp_queue, &filtered);
    }
}

void task_controller(void)
{
    temp_zone_t last_zone = TEMP_ZONE_NORMAL;
    uint32_t last_log_tick = 0U;

    while (1) {
        int filtered = 0;
        int fan_pwm = 0;
        temp_zone_t zone = TEMP_ZONE_NORMAL;

        msg_queue_receive(&temp_queue, &filtered);

        if (filtered >= APP_TEMP_CRITICAL_THRESHOLD) {
            zone = TEMP_ZONE_CRITICAL;
            fan_pwm = 100;
        } else if (filtered >= APP_TEMP_WARN_THRESHOLD) {
            zone = TEMP_ZONE_WARN;
            fan_pwm = 60;
        }

        app_record_control(zone, fan_pwm);

        if (zone != last_zone) {
            if (zone == TEMP_ZONE_NORMAL) {
                app_print_line("[ALARM] Thermal state returned to normal.");
            } else if (mutex_lock_timeout(&app_mutex, 20) == OS_OK) {
                uart_print("[ALARM] zone=");
                uart_print(app_temp_zone_str(zone));
                uart_print(" filtered=");
                uart_print_dec((uint32_t)filtered);
                uart_print("C\r\n");
                mutex_unlock(&app_mutex);
            }
            last_zone = zone;
        }

        if ((uint32_t)(os_tick_count - last_log_tick) >= APP_LOG_PERIOD_TICKS) {
            app_thermal_snapshot_t snapshot;

            last_log_tick = os_tick_count;
            if (app_get_thermal_snapshot(&snapshot, 5) == OS_OK &&
                mutex_lock_timeout(&app_mutex, 20) == OS_OK) {
                uart_print("[TEMP] raw=");
                uart_print_dec((uint32_t)snapshot.raw_temperature);
                uart_print("C filtered=");
                uart_print_dec((uint32_t)snapshot.filtered_temperature);
                uart_print("C zone=");
                uart_print(app_temp_zone_str(snapshot.zone));
                uart_print(" fan=");
                uart_print_dec((uint32_t)snapshot.fan_pwm_percent);
                uart_print("%\r\n");
                mutex_unlock(&app_mutex);
            }
        }
    }
}
