#include "app_global.h"
#include "banker.h"
#include "kernel.h"
#include "uart.h"

static void configure_storage_claims(void)
{
    int claims[] = {1, 0, 1};
    (void)os_resource_set_claims(claims);
}

void task_temp_storage(void)
{
    int dma_request[] = {0, 0, 1};

    configure_storage_claims();

    while (1) {
        os_delay(250);

        if (!request_resources(dma_request)) {
            app_print_line("[STORAGE] DMA unavailable, record skipped.");
            continue;
        }

        if (mutex_lock_timeout(&storage_mutex, 20) == OS_OK) {
            app_thermal_snapshot_t snapshot;

            if (app_get_thermal_snapshot(&snapshot, 5) == OS_OK &&
                mutex_lock_timeout(&app_mutex, 20) == OS_OK) {
                uart_print("[STORAGE] saved temp=");
                uart_print_dec((uint32_t)snapshot.filtered_temperature);
                uart_print("C min=");
                uart_print_dec((uint32_t)snapshot.min_temperature);
                uart_print("C max=");
                uart_print_dec((uint32_t)snapshot.max_temperature);
                uart_print("C\r\n");
                mutex_unlock(&app_mutex);
            }

            os_delay(10);
            mutex_unlock(&storage_mutex);
        }

        release_resources(dma_request);

        if (mutex_lock_timeout(&temp_state_mutex, 5) == OS_OK) {
            storage_cycles++;
            mutex_unlock(&temp_state_mutex);
        }
    }
}

void task_health_monitor(void)
{
    uint32_t missed_heartbeats = 0;

    while (1) {
        os_status_t status = sem_wait_timeout(&heartbeat_sem, 300);

        if (status == OS_TIMEOUT) {
            missed_heartbeats++;
            if (mutex_lock_timeout(&app_mutex, 20) == OS_OK) {
                uart_print("[HEALTH] missed heartbeat count=");
                uart_print_dec(missed_heartbeats);
                uart_print("\r\n");
                mutex_unlock(&app_mutex);
            }
        }

        task_check_all_stacks();

        app_thermal_snapshot_t snapshot;
        if (app_get_thermal_snapshot(&snapshot, 5) == OS_OK &&
            snapshot.sensor_samples != 0U &&
            (snapshot.sensor_samples % 32U) == 0U) {
            app_print_system_status();
            os_delay(1);
        }
    }
}

void task_resource_user_a(void)
{
    task_temp_storage();
}

void task_resource_user_b(void)
{
    task_health_monitor();
}

void task_deadlock_1(void)
{
    task_temp_storage();
}

void task_deadlock_2(void)
{
    task_health_monitor();
}

void task_banker1(void)
{
    task_temp_storage();
}

void task_banker2(void)
{
    task_health_monitor();
}
