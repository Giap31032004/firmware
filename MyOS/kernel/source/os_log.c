#include "os_log.h"

__attribute__((weak))
void os_log_backend_putc(char c)
{
    (void)c;
}

void os_log_putc(char c)
{
    os_log_backend_putc(c);
}

void os_log_write(const char *s)
{
    if (s == 0) {
        return;
    }

    while (*s != '\0') {
        os_log_putc(*s++);
    }
}

void os_log_write_dec(uint32_t value)
{
    char buf[10];
    uint32_t i = 0U;

    if (value == 0U) {
        os_log_putc('0');
        return;
    }

    while (value > 0U && i < sizeof(buf)) {
        buf[i++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (i > 0U) {
        os_log_putc(buf[--i]);
    }
}

static char os_log_hex_nibble(uint8_t nibble)
{
    return (nibble < 10U) ? (char)('0' + nibble)
                          : (char)('A' + (nibble - 10U));
}

void os_log_write_hex8(uint8_t value)
{
    os_log_putc(os_log_hex_nibble((uint8_t)((value >> 4U) & 0x0FU)));
    os_log_putc(os_log_hex_nibble((uint8_t)(value & 0x0FU)));
}

void os_log_write_hex32(uint32_t value)
{
    os_log_write("0x");
    for (uint32_t i = 0U; i < 8U; i++) {
        uint8_t nibble = (uint8_t)((value >> (28U - (i * 4U))) & 0x0FU);
        os_log_putc(os_log_hex_nibble(nibble));
    }
}

void os_log_write_column(const char *text, uint32_t width)
{
    uint32_t length = 0U;

    if (text == 0) {
        text = "";
    }

    while (text[length] != '\0' && length < width) {
        os_log_putc(text[length]);
        length++;
    }

    while (length < width) {
        os_log_putc(' ');
        length++;
    }
}
