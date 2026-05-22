#include "hal_gpio.h"
#include "bsp_gpio.h"
#include "driver_gpio.h"

void hal_led_init(void)
{
    gpio_driver_init_output(LED1_PORT, LED1_PIN);
}

void hal_led_write(hal_gpio_state_t state)
{
    gpio_driver_write(LED1_PORT,
                      LED1_PIN,
                      state);
}

void hal_led_toggle(void)
{
    gpio_driver_toggle(LED1_PORT,
                       LED1_PIN);
}