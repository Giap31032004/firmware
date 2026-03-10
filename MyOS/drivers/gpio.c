#include "gpio.h"
#include <stdint.h>

/* --- HÀM NỘI BỘ (Private Helper) --- */
static uint32_t get_rcgc2_mask(uint32_t port_base) {
    switch (port_base) {
        case GPIO_PORTA_BASE: return 0x01; // Bit 0
        case GPIO_PORTB_BASE: return 0x02; // Bit 1
        case GPIO_PORTC_BASE: return 0x04; // Bit 2
        case GPIO_PORTD_BASE: return 0x08; // Bit 3
        case GPIO_PORTE_BASE: return 0x10; // Bit 4
        case GPIO_PORTF_BASE: return 0x20; // Bit 5
        case GPIO_PORTG_BASE: return 0x40; // Bit 6
        default: return 0;
    }
}

/* --- PUBLIC FUNCTIONS --- */

void gpio_init(uint32_t port_base, uint8_t pin_mask, uint8_t direction) {
    uint32_t rcgc_mask = get_rcgc2_mask(port_base);
    if (rcgc_mask == 0) return; 
    SYSCTL_RCGC2 |= rcgc_mask;

    volatile uint32_t delay = SYSCTL_RCGC2; 
    (void)delay; 

    volatile uint32_t *gpio_dir = (uint32_t *)(port_base + GPIO_DIR_OFFSET);
    
    if (direction == GPIO_DIR_OUTPUT) {
        *gpio_dir |= pin_mask; // Set bit thành 1 (Output)
    } else {
        *gpio_dir &= ~pin_mask; // Clear bit thành 0 (Input)
    }

    // 6. Tắt chức năng thay thế (AFSEL)
    volatile uint32_t *gpio_afsel = (uint32_t *)(port_base + GPIO_AFSEL_OFFSET);
    *gpio_afsel &= ~pin_mask;

    // 7. Bật Digital Enable (DEN)
    volatile uint32_t *gpio_den = (uint32_t *)(port_base + GPIO_DEN_OFFSET);
    *gpio_den |= pin_mask;
}

void gpio_write(uint32_t port_base, uint8_t pin_mask, uint8_t value) {
    volatile uint32_t *gpio_data_masked = (uint32_t *)(port_base + (pin_mask << 2));
    
    if (value) {
        *gpio_data_masked = 0xFF; // Ghi bất kỳ giá trị khác 0 nào vào vị trí mask cũng được
    } else {
        *gpio_data_masked = 0x00;
    }
}

void gpio_toggle(uint32_t port_base, uint8_t pin_mask) {
    volatile uint32_t *gpio_data = (uint32_t *)(port_base + (pin_mask << 2));
    *gpio_data ^= 0xFF; // Đảo bit
}

uint32_t gpio_read(uint32_t port_base, uint8_t pin_mask) {
    volatile uint32_t *gpio_data = (uint32_t *)(port_base + (pin_mask << 2));
    return (*gpio_data != 0) ? 1 : 0;
}