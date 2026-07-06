#include "hal_uart.h"

#include "driver_uart.h"
#include "kernel.h"
#include "os_trace.h"
#include "sync.h"

#define RX_BUFFER_SIZE 128

static volatile char rx_buffer[RX_BUFFER_SIZE];
static volatile int rx_head = 0;
static volatile int rx_tail = 0;

static os_sem_t uart_rx_semaphore;
static os_mutex_t uart_tx_mutex;

void uart_init(void)
{
    binary_sem_init(&uart_rx_semaphore, 0);
    mutex_init(&uart_tx_mutex);
    os_trace_ignore_mutex(&uart_tx_mutex);

    uart_driver_init();
}

void uart_putc_raw(char c)
{
    uart_driver_putc_raw(c);
}

void os_log_backend_putc(char c)
{
    if (c == '\n') {
        uart_putc_raw('\r');
    }

    uart_putc_raw(c);
}

void uart_putc(char c)
{
    mutex_lock(&uart_tx_mutex);
    uart_putc_raw(c);
    mutex_unlock(&uart_tx_mutex);
}

void uart_print(const char *s)
{
    if (s == 0) {
        return;
    }

    mutex_lock(&uart_tx_mutex);
    while (*s != '\0') {
        if (*s == '\n') {
            uart_putc_raw('\r');
        }
        uart_putc_raw(*s++);
    }
    mutex_unlock(&uart_tx_mutex);
}

char uart_getc(void)
{
    char c;

    sem_wait(&uart_rx_semaphore);

    OS_ENTER_CRITICAL();
    c = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;
    OS_EXIT_CRITICAL();

    return c;
}

void uart_driver_rx_callback_from_isr(char c)
{
    int next_head = (rx_head + 1) % RX_BUFFER_SIZE;

    if (next_head != rx_tail) {
        rx_buffer[rx_head] = c;
        rx_head = next_head;
        sem_signal_from_isr(&uart_rx_semaphore);
    }
}

int _write(int file, char *ptr, int len)
{
    (void)file;

    mutex_lock(&uart_tx_mutex);
    for (int i = 0; i < len; i++) {
        if (ptr[i] == '\n') {
            uart_putc_raw('\r');
        }
        uart_putc_raw(ptr[i]);
    }
    mutex_unlock(&uart_tx_mutex);

    return len;
}

static char nibble_to_hex(uint8_t nibble)
{
    return (nibble < 10U) ? (char)('0' + nibble)
                          : (char)('A' + (nibble - 10U));
}

void uart_print_hex(uint8_t n)
{
    char str[3];

    str[0] = nibble_to_hex((uint8_t)((n >> 4U) & 0x0FU));
    str[1] = nibble_to_hex((uint8_t)(n & 0x0FU));
    str[2] = '\0';
    uart_print(str);
}

void uart_print_dec(uint32_t val)
{
    char buf[12];
    int i = 0;

    if (val == 0U) {
        uart_putc('0');
        return;
    }

    while (val > 0U) {
        buf[i++] = (char)('0' + (val % 10U));
        val /= 10U;
    }

    mutex_lock(&uart_tx_mutex);
    while (i > 0) {
        uart_putc_raw(buf[--i]);
    }
    mutex_unlock(&uart_tx_mutex);
}

void uart_print_hex32(uint32_t n)
{
    char str[11];

    str[0] = '0';
    str[1] = 'x';
    for (int i = 0; i < 8; i++) {
        uint8_t nibble = (uint8_t)((n >> (28U - ((uint32_t)i * 4U))) & 0x0FU);
        str[2 + i] = nibble_to_hex(nibble);
    }
    str[10] = '\0';
    uart_print(str);
}
