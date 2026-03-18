#include "task.h"
#include "kernel.h"     // Để dùng các hàm của OS (như os_delay)
#include "gpio.h"       // Driver GPIO chuẩn (đã rút ruột 0x400...)
#include "memory_map.h" // "Sổ đỏ" chứa địa chỉ GPIOA_BASE
#include <stdint.h>
#include <stddef.h>     

/* ====================================================================
 * TASK NHÁY ĐÈN (Chuẩn RTOS)
 * ==================================================================== */
void task_gpio_blink(void) {
    /* 1. Cấu hình GPIO (GPIOA, Pin 5 - Thường là LED xanh trên board STM32) 
          Khởi tạo thành Output */
    gpio_init(GPIOA_BASE, GPIO_PIN_5, GPIO_DIR_OUTPUT);

    /* 2. Vòng lặp vô tận của task */
    while (1) {
        /* Đảo trạng thái đèn LED */
        gpio_toggle(GPIOA_BASE, GPIO_PIN_5);
        
        /* [QUAN TRỌNG] Bắt Task đi ngủ trong 500 tick (500ms).
           Trong thời gian ngủ này, Kernel sẽ thu hồi CPU để chạy Task khác 
           hoặc rơi vào wfi (Idle) để tiết kiệm pin! */
           
        // Tùy vào tên hàm delay bạn viết trong OS, có thể là os_delay hoặc sys_delay
        // Ở đây mình dùng os_delay theo quy ước chuẩn.
        os_delay(500); 
    }
}