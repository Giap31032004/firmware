#ifndef DRIVER_UART_H
#define DRIVER_UART_H

#include <stdint.h>

void uart_driver_init(void);
void uart_driver_putc_raw(char c);
void uart_driver_rx_callback_from_isr(char c);

#endif /* DRIVER_UART_H */
