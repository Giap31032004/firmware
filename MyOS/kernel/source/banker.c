#include "banker.h"
#include "critical.h"
#include "task.h"
#include "uart.h"

int system_available[NUM_RESOURCES];

void banker_init(void)
{
    system_available[RES_UART] = 1;
    system_available[RES_I2C] = 1;
    system_available[RES_DMA_CH] = 2;
}

static int is_safe_state(void)
{
    int work[NUM_RESOURCES];
    int finish[MAX_TASKS];

    for (int i = 0; i < NUM_RESOURCES; i++) {
        work[i] = system_available[i];
    }

    for (int i = 0; i < MAX_TASKS; i++) {
        TCB_t *task = &tcb_table[i];
        if (task->state == TASK_UNUSED ||
            task->state == TASK_NEW ||
            task->state == TASK_TERMINATED ||
            task->state == TASK_SUSPENDED) {
            finish[i] = 1;
        } else {
            finish[i] = 0;
        }
    }

    while (1) {
        int found_task = 0;

        for (int i = 0; i < MAX_TASKS; i++) {
            TCB_t *task = &tcb_table[i];

            if (finish[i] == 0) {
                int can_allocate = 1;

                for (int r = 0; r < NUM_RESOURCES; r++) {
                    int need = task->res.max[r] - task->res.held[r];
                    if (need > work[r]) {
                        can_allocate = 0;
                        break;
                    }
                }

                if (can_allocate) {
                    for (int r = 0; r < NUM_RESOURCES; r++) {
                        work[r] += task->res.held[r];
                    }
                    finish[i] = 1;
                    found_task = 1;
                }
            }
        }

        if (!found_task) {
            break;
        }
    }

    for (int i = 0; i < MAX_TASKS; i++) {
        if (finish[i] == 0) {
            return 0;
        }
    }

    return 1;
}

int request_resources(int request[])
{
    TCB_t *task = current_tcb;
    if (task == NULL) {
        return 0;
    }

    uint32_t irq_state = os_enter_critical();

    for (int i = 0; i < NUM_RESOURCES; i++) {
        int need = task->res.max[i] - task->res.held[i];
        if (request[i] > need) {
            uart_print("Banker: Error! Request > Need.\r\n");
            os_exit_critical(irq_state);
            return 0;
        }

        if (request[i] > system_available[i]) {
            os_exit_critical(irq_state);
            return 0;
        }
    }

    for (int i = 0; i < NUM_RESOURCES; i++) {
        system_available[i] -= request[i];
        task->res.held[i] += request[i];
    }

    if (is_safe_state()) {
        os_exit_critical(irq_state);
        return 1;
    }

    for (int i = 0; i < NUM_RESOURCES; i++) {
        system_available[i] += request[i];
        task->res.held[i] -= request[i];
    }

    os_exit_critical(irq_state);
    return 0;
}

void release_resources(int release[])
{
    TCB_t *task = current_tcb;
    if (task == NULL) {
        return;
    }

    uint32_t irq_state = os_enter_critical();

    for (int i = 0; i < NUM_RESOURCES; i++) {
        task->res.held[i] -= release[i];
        system_available[i] += release[i];
    }

    os_exit_critical(irq_state);
}
