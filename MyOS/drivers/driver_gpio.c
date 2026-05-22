#include "driver_gpio.h"

void gpio_driver_init_output(GPIO_TypeDef *port, uint8_t pin)
{
    if(port == GPIOA)
        RCC->AHB1ENR |= (1 << 0);

    port->MODER &= ~(3 << (pin * 2));
    port->MODER |=  (1 << (pin * 2));
}

void gpio_driver_write(GPIO_TypeDef *port,
                       uint8_t pin,
                       uint8_t value)
{
    if(value)
        port->ODR |= (1 << pin);
    else
        port->ODR &= ~(1 << pin);
}

uint8_t gpio_driver_read(GPIO_TypeDef *port,
                         uint8_t pin)
{
    return (port->IDR >> pin) & 0x1;
}

void gpio_driver_toggle(GPIO_TypeDef *port,
                        uint8_t pin)
{
    port->ODR ^= (1 << pin);
}