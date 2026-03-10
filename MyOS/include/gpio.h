#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>

/* --- SYSTEM CONTROL (Clock Gating) --- */
/* Base address for System Control usually 0x400FE000 on LM3S/TM4C */
#define SYSCTL_RCGC2        *((volatile uint32_t *)0x400FE108) 

/* --- GPIO BASE ADDRESSES (LM3S Memory Map) --- */
#define GPIO_PORTA_BASE     0x40004000
#define GPIO_PORTB_BASE     0x40005000
#define GPIO_PORTC_BASE     0x40006000
#define GPIO_PORTD_BASE     0x40007000
#define GPIO_PORTE_BASE     0x40024000
#define GPIO_PORTF_BASE     0x40025000 
#define GPIO_PORTG_BASE     0x40026000

/* --- GPIO REGISTER OFFSETS --- */
/* These are the distances from the PORT_BASE to the specific register */
#define GPIO_DATA_OFFSET    0x000
#define GPIO_DIR_OFFSET     0x400
#define GPIO_AFSEL_OFFSET   0x420
#define GPIO_DEN_OFFSET     0x51C

/* --- PIN DEFINITIONS (Bit Masks) --- */
#define GPIO_PIN_0          (1U << 0)
#define GPIO_PIN_1          (1U << 1)
#define GPIO_PIN_2          (1U << 2)
#define GPIO_PIN_3          (1U << 3)
#define GPIO_PIN_4          (1U << 4)
#define GPIO_PIN_5          (1U << 5)
#define GPIO_PIN_6          (1U << 6)
#define GPIO_PIN_7          (1U << 7)

/* --- DIRECTION DEFINITIONS --- */
#define GPIO_DIR_INPUT      0
#define GPIO_DIR_OUTPUT     1

/* --- PUBLIC FUNCTIONS --- */
void gpio_init(uint32_t port_base, uint8_t pin_mask, uint8_t direction);
void gpio_write(uint32_t port_base, uint8_t pin_mask, uint8_t value);
void gpio_toggle(uint32_t port_base, uint8_t pin_mask);
uint32_t gpio_read(uint32_t port_base, uint8_t pin_mask);

#endif