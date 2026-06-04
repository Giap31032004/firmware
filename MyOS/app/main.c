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
      1. Board peripherals
    ------------------------------------------*/
    board_init();
    // uart
    // gpio
    // spi/i2c nếu cần
    // driver-level IRQ enable nếu có


    /*------------------------------------------
      2. Kernel init
    ------------------------------------------*/
    kernel_init();
    // ready queue
    // scheduler state
    // idle task

    /*------------------------------------------
      3. Optional services
    ------------------------------------------*/
    service_init();
    // mutex
    // sem
    // ipc


    /*------------------------------------------
      4. Create user tasks
    ------------------------------------------*/
    app_init();
    // task_create(...)
    // lúc này mới có thể setup
    // per-task MPU regions nếu dùng


    /*------------------------------------------
      5. Start RTOS
    ------------------------------------------*/
    os_start();
    // SysTick init
    // PendSV priority
    // __enable_irq()
    // first task
    while(1);
}
