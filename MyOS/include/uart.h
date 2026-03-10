#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "sync.h" 

/* --- CẤU HÌNH --- */
#define SYSTEM_CLOCK_HZ    50000000U  
#define UART_BAUDRATE    115200U

/* --- PUBLIC FUNCTIONS --- */
void uart_init(void);

// Hàm gửi 1 ký tự (có chờ nếu FIFO đầy)
void uart_putc(char c);

// Hàm nhận 1 ký tự (Block task nếu buffer rỗng)
char uart_getc(void);

// Hàm in chuỗi (Thread-safe: Đã bọc Mutex)
void uart_print(const char *s);

// Hàm in số (Legacy support)
void uart_print_dec(uint32_t val);
void uart_print_hex(uint8_t n);
void uart_print_hex32(uint32_t n);
void uart_putc_raw(char c);
#endif // UART_H