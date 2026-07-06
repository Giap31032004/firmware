#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>
#include "stm32f407xx.h"

/* =========================================================
   Board Support Package (BSP)

   Target:
   STM32F4 Discovery
   ========================================================= */


/* =========================================================
   Board Hardware Definitions
   ========================================================= */

/* -------------------------
   On-board LEDs (Port D)
   ------------------------- */
#define BOARD_LED_PORT         GPIOD_BASE

#define BOARD_LED_GREEN_PIN    12
#define BOARD_LED_ORANGE_PIN   13
#define BOARD_LED_RED_PIN      14
#define BOARD_LED_BLUE_PIN     15


/* -------------------------
   User Button (PA0)
   ------------------------- */
#define BOARD_BUTTON_PORT      GPIOA_BASE
#define BOARD_BUTTON_PIN       0


/* -------------------------
   Console UART
   ------------------------- */
#define BOARD_CONSOLE_UART     USART1_BASE



/* =========================================================
   Board Initialization API
   ========================================================= */

/*
 * High-level board bring-up
 *
 * Gọi trực tiếp các module init:
 *
 *   mpu_init()
 *   uart_init()
 *   gpio_init()
 *
 * Tương tự cách:
 *
 *   SystemInit()
 *   {
 *      scb_init();
 *      rcc_init();
 *   }
 */
void board_init(void);


#endif /* BOARD_H */
