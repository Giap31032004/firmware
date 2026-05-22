#include "i2c.h"
#include "kernel.h"
#include "task.h"
#include "uart.h"

void task_i2c_scanner(void)
{
    uart_print("[I2C] Task started.\r\n");
    i2c_init();

    while (1) {
        bool found = i2c_write_byte(0x3C, 0x00, 0x00);

        if (found) {
            uart_print("[I2C] Found device at 0x3C.\r\n");
        }

        os_delay(1000);
    }
}
