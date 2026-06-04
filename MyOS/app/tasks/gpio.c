#include "task.h"
#include "kernel.h"     // Để dùng các hàm của OS (như os_delay)
#include "hal_gpio.h"
#include <stdint.h>
#include <stddef.h>     

/* ====================================================================
 * TASK NHÁY ĐÈN (Chuẩn RTOS)
 * ==================================================================== */
void task_gpio_blink(void) {
    /* 1. Cấu hình GPIO (GPIOA, Pin 5 - Thường là LED xanh trên board STM32) 
          Khởi tạo thành Output */
    hal_led_init();

    /* 2. Vòng lặp vô tận của task */
    while (1) {
        /* Đảo trạng thái đèn LED */
        hal_led_toggle();
        
        /* [QUAN TRỌNG] Bắt Task đi ngủ trong 500 tick (500ms).
           Trong thời gian ngủ này, Kernel sẽ thu hồi CPU để chạy Task khác 
           hoặc rơi vào wfi (Idle) để tiết kiệm pin! */
           
        // Tùy vào tên hàm delay bạn viết trong OS, có thể là os_delay hoặc sys_delay
        // Ở đây mình dùng os_delay theo quy ước chuẩn.
        os_delay(500); 
    }
}
