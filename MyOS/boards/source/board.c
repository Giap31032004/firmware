#include "board.h"
#include "mpu.h"
#include "uart.h"
#include "hal_gpio.h"
#include "system_stm32f4xx.h"

void board_init(void)
{
    mpu_init();

    uart_init();

    hal_led_init();

}
