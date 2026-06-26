#ifndef OS_LOG_H
#define OS_LOG_H

#include <stdint.h>

void os_log_write(const char *s);
void os_log_putc(char c);
void os_log_write_dec(uint32_t value);
void os_log_write_hex8(uint8_t value);
void os_log_write_hex32(uint32_t value);
void os_log_write_column(const char *text, uint32_t width);

#endif /* OS_LOG_H */
