#include "uart.h"
#include "kernel.h"
#include "task.h"
#include "sync.h"
#include "ipc.h"
#include <stdint.h>
#include <stddef.h>
#include "app_global.h"
#include "board.h"

/* ================================================= */
/* MAIN FUNCTION                                     */
/* ================================================= */

int main(void)
{
    /*------------------------------------------
      1. Board and low-level hardware init
    ------------------------------------------*/
    board_init();
    // MPU static regions
    // UART
    // GPIO/LED

    /*------------------------------------------
      2. Kernel init
    ------------------------------------------*/
    kernel_init();
    // runtime statistics and trace buffer
    // scheduler ready queues
    // task table, tick, timer, heap
    // idle task

    /*------------------------------------------
      3. Application services
    ------------------------------------------*/
    service_init();
    // temperature message queue
    // application mutex

    /*------------------------------------------
      4. Create user tasks
    ------------------------------------------*/
    app_init();
    // task_create_dynamic(...)
    // per-task stack and MPU settings

    /*------------------------------------------
      5. Start RTOS
    ------------------------------------------*/
    os_start();
    // select first ready task
    // load first task MPU settings
    // set PendSV/SysTick/SVC priorities
    // start SysTick
    // SVC enters the first task

    while (1) {
    }
}
