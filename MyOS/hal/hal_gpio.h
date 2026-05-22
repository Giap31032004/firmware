#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdint.h>

typedef enum
{
    HAL_GPIO_LOW = 0,
    HAL_GPIO_HIGH
} hal_gpio_state_t;

void hal_led_init(void);

void hal_led_write(hal_gpio_state_t state);

void hal_led_toggle(void);

#endif