#include "uart.h"
#include "systick.h"
#include "kernel.h"
#include "task.h"       
#include "sync.h" 
#include "mpu.h"
#include "ipc.h"
#include "gpio.h"

#include <stdint.h>
#include <stddef.h>   

/* ================================================= */
/* CẤU HÌNH HỆ THỐNG                                 */
/* ================================================= */
#define SYSTEM_CLOCK      50000000 
#define SYSTICK_RATE      10000000 



/* ================================================= */
/* DỮ LIỆU RIÊNG CHO CÁC TASK                        */
/* ================================================= */
int max_res_t1[] = {0, 0, 2}; 
int max_res_t2[] = {0, 0, 2};

/* ================================================= */
/* MAIN FUNCTION                                     */
/* ================================================= */

/* Hàm delay đơn giản */
void delay(volatile unsigned int count) {
    while (count--) {
        __asm("nop");
    }
}

int main(void) {
    /* Khởi tạo phần cứng cơ bản */
    uart_init();
    
    /* Khởi tạo Kernel */
    os_kernel_init();

    /* Khởi tạo Tài nguyên (IPC & Sync) */
    msg_queue_init(&temp_queue);
    mutex_init(&app_mutex);
    mutex_init(&mutex_A);
    mutex_init(&mutex_B);

    /* In thông báo khởi động */
    uart_print("\033[2J"); // Xóa màn hình terminal
    uart_print("MyOS IoT System Booting...\r\n");
    
    /* Tạo các Task */
    process_create(task_sensor_update, 1, 4, NULL); 
    process_create(task_display,       2, 2, NULL);       
    process_create(task_alarm,         3, 3, NULL);         
    process_create(task_logger,        4, 4, NULL);              
    process_create(task_shell,         5, 1, NULL);
    process_create(task_deadlock_1,    6, 5, NULL);
    process_create(task_deadlock_2,    7, 5, NULL);
    process_create(task_banker1,       8, 4, max_res_t1);
    process_create(task_banker2,       9, 4, max_res_t2);

    // --- Nhóm Test Drivers ---
    // process_create(task_gpio_blink, 10, 5, NULL); // Task nháy đèn (Tạm tắt)
    //process_create(task_i2c_scanner,   10, 5, NULL); // Task quét I2C (Mới thêm)
    //process_create(task_dma_test, 10, 5, NULL); // Task test DMA

    /* Khởi động System Tick ,nhịp tim hệ điều hành */
    systick_init(SYSTICK_RATE); 

    /* 7. Vòng lặp Idle */
    while (1) {
        // CPU sẽ chạy vào đây khi không có task nào khác hoạt động
        // Có thể đưa CPU vào chế độ ngủ (Sleep mode) để tiết kiệm điện
        // __asm("wfi"); // Wait For Interrupt
    }
}