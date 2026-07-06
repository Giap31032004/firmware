#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdint.h>

#define SYSTEM_CLOCK_HZ 16000000U
#define UART_BAUDRATE   115200U

void uart_init(void);
void uart_putc(char c);
char uart_getc(void);
void uart_print(const char *s);
void uart_print_dec(uint32_t val);
void uart_print_hex(uint8_t n);
void uart_print_hex32(uint32_t n);
void uart_putc_raw(char c);

#endif /* HAL_UART_H */
