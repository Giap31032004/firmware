#ifndef DRIVER_UART_H
#define DRIVER_UART_H

#include <stdint.h>

#define UART_DRIVER_CLOCK_HZ 16000000U
#define UART_DRIVER_BAUDRATE 115200U

void uart_driver_init(void);
void uart_driver_putc_raw(char c);
void uart_driver_rx_callback_from_isr(char c);

#endif /* DRIVER_UART_H */
