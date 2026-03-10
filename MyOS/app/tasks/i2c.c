#include "task.h"       // Include file API chung
#include "i2c.h"        // Driver I2C
#include "uart.h"       // Để in log

/* Hàm delay phụ trợ */
static void delay_loop(volatile uint32_t count) {
    while(count--) { __asm("nop"); }
}

/* Nội dung Task */
void task_i2c_scanner(void) {
    uart_print("[I2C] Task started...\n");
    i2c_init(); // Khởi tạo phần cứng I2C
    
    while(1) {
        // uart_print("[I2C] Scanning addr 0x3C...\n");
        
        // Thử ghi vào địa chỉ 0x3C (OLED)
        bool found = i2c_write_byte(0x3C, 0x00, 0x00);
        
        if (found) {
            uart_print("[I2C] Found device at 0x3C!\n");
        }
        
        // Delay (Giả lập sleep 1 giây)
        delay_loop(5000000); 
    }
}