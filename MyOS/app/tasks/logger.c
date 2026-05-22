#include "app_global.h"
#include "kernel.h"
#include "uart.h"

void task_logger(void)
{
    uint32_t counter = 0;

    while (1) {
        os_status_t status = sem_wait_timeout(&heartbeat_sem, 100);

        if (mutex_lock_timeout(&app_mutex, 20) == OS_OK) {
            uart_print("    >>> [LOGGER] count=");
            uart_print_dec(counter++);
            uart_print(" heartbeat=");
            uart_print(status == OS_OK ? "yes" : "timeout");
            uart_print("\r\n");
            mutex_unlock(&app_mutex);
        }
    }
}
