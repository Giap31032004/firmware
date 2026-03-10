#include "task.h"
#include "gpio.h"       // Driver GPIO của bạn
#include <stdint.h>
#include <stddef.h>     // Để dùng NULL

// Hàm delay cục bộ cho task này
static void delay_loop(volatile uint32_t count) {
    while(count--) {
        __asm("nop");
    }
}

// Đây là hàm sẽ được chạy bởi process_create
// Signature hàm phải khớp với định nghĩa trong OS của bạn (thường là void hoặc void*)
void task_gpio_blink(void) {
    // 1. Cấu hình GPIO (Port F, Pin 2 - LED)
    // Nếu main đã init rồi thì không cần, nhưng init lại ở đây cho chắc chắn
    gpio_init(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_DIR_OUTPUT);

    // 2. Vòng lặp vô tận của task
    while (1) {
        // Toggle đèn
        gpio_toggle(GPIO_PORTF_BASE, GPIO_PIN_2);
        
        // Delay (Giả lập work load)
        delay_loop(500000);
        
        // Lưu ý: Trong OS thật, chỗ này nên gọi syscall sleep hoặc yield
        // Ví dụ: sys_delay(500); 
    }
}