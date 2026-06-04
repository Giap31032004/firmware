#include "app_global.h"
#include "kernel.h"
#include "uart.h"

void task_temp_logger(void)
{
    uint32_t batch = 0;

    while (1) {
        int filtered = 0;

        msg_queue_receive(&temp_log_queue, &filtered);

        if ((batch++ % APP_LOG_SAMPLE_INTERVAL) != 0U) {
            log_records++;
            continue;
        }

        app_thermal_snapshot_t snapshot;
        if (app_get_thermal_snapshot(&snapshot, 5) != OS_OK) {
            continue;
        }

        if (mutex_lock_timeout(&app_mutex, 20) == OS_OK) {
            uart_print("[TEMP-LOG] filtered=");
            uart_print_dec((uint32_t)filtered);
            uart_print("C zone=");
            uart_print(app_temp_zone_str(snapshot.zone));
            uart_print(" samples=");
            uart_print_dec(snapshot.sensor_samples);
            uart_print("\r\n");
            log_records++;
            mutex_unlock(&app_mutex);
        }
    }
}

void task_logger(void)
{
    task_temp_logger();
}
