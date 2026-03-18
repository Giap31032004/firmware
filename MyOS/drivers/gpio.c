#include "gpio.h"
#include "memory_map.h" /* Chuẩn bị sẵn la bàn của STM32F407 cho tương lai */
#include <stdint.h>

/* --- PUBLIC FUNCTIONS (STUBS) --- */

void gpio_init(uint32_t port_base, uint16_t pin_mask, uint8_t direction) {
    /* TODO: Viết phần thực thi cấu hình thanh ghi MODER, OTYPER của STM32F4 sau */
    return;
}

void gpio_write(uint32_t port_base, uint16_t pin_mask, uint8_t value) {
    /* TODO: Viết phần thực thi ghi vào thanh ghi BSRR/ODR của STM32F4 sau */
    return;
}

void gpio_toggle(uint32_t port_base, uint16_t pin_mask) {
    /* TODO: Viết phần thực thi đảo bit thanh ghi ODR của STM32F4 sau */
    return;
}

uint32_t gpio_read(uint32_t port_base, uint16_t pin_mask) {
    /* TODO: Viết phần thực thi đọc thanh ghi IDR của STM32F4 sau */
    return 0;
}