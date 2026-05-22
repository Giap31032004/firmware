#ifndef DRIVER_GPIO_H
#define DRIVER_GPIO_H

#include <stdint.h>
#include "stm32f407xx.h"
void gpio_driver_init_output(GPIO_TypeDef* port, uint8_t pin);

void gpio_driver_write(GPIO_TypeDef* port, uint8_t pin, uint8_t value);

uint8_t gpio_driver_read(GPIO_TypeDef* port, uint8_t pin);

void gpio_driver_toggle(GPIO_TypeDef* port, uint8_t pin);

#endif