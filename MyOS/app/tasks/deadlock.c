#include "app_global.h"
#include "banker.h"
#include "kernel.h"
#include "scheduler.h"
#include "uart.h"

static void configure_resource_claims(void)
{
    if (current_tcb == NULL) {
        return;
    }

    current_tcb->res.max[RES_UART] = 1;
    current_tcb->res.max[RES_I2C] = 0;
    current_tcb->res.max[RES_DMA_CH] = 1;
}

void task_resource_user_a(void)
{
    int dma_request[] = {0, 0, 1};

    configure_resource_claims();

    while (1) {
        os_delay(75);

        if (request_resources(dma_request)) {
            if (mutex_lock_timeout(&mutex_A, 20) == OS_OK) {
                app_print_line("[RES-A] DMA granted, protected work started.");
                os_delay(30);
                mutex_unlock(&mutex_A);
            }

            release_resources(dma_request);

            scheduler_lock();
            resource_cycles++;
            scheduler_unlock();
        } else {
            app_print_line("[RES-A] DMA denied by Banker.");
        }
    }
}

void task_resource_user_b(void)
{
    int dma_request[] = {0, 0, 1};

    configure_resource_claims();

    while (1) {
        os_delay(95);

        if (request_resources(dma_request)) {
            os_status_t lock_result = mutex_lock_timeout(&mutex_A, 10);

            if (lock_result == OS_OK) {
                app_print_line("[RES-B] DMA granted, mutex acquired.");
                os_delay(15);
                mutex_unlock(&mutex_A);
            } else {
                app_print_line("[RES-B] Mutex timeout while holding DMA.");
            }

            release_resources(dma_request);

            scheduler_lock();
            resource_cycles++;
            scheduler_unlock();
        } else {
            app_print_line("[RES-B] DMA denied by Banker.");
        }
    }
}

void task_health_monitor(void)
{
    while (1) {
        os_status_t status = sem_wait_timeout(&heartbeat_sem, 250);

        if (status == OS_TIMEOUT) {
            app_print_line("[HEALTH] Heartbeat timeout.");
        }

        task_check_all_stacks();

        if ((sensor_samples % 20U) == 0U) {
            app_print_system_status();
        }
    }
}

void task_deadlock_1(void)
{
    task_resource_user_a();
}

void task_deadlock_2(void)
{
    task_resource_user_b();
}

void task_banker1(void)
{
    task_resource_user_a();
}

void task_banker2(void)
{
    task_resource_user_b();
}
