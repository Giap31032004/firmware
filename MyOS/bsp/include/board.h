#ifndef BOARD_H
#define BOARD_H

/* Nhập khẩu bản đồ địa chỉ của chip */
#include "memory_map.h"

/* =================================================================
 * ĐỊNH NGHĨA PHẦN CỨNG BO MẠCH STM32F4 DISCOVERY
 * ================================================================= */

/* 1. Đèn LED: Bo mạch này có 4 con LED ở Port D */
#define BOARD_LED_PORT        GPIOD_BASE
#define BOARD_LED_GREEN_PIN   12
#define BOARD_LED_ORANGE_PIN  13
#define BOARD_LED_RED_PIN     14
#define BOARD_LED_BLUE_PIN    15

/* 2. Nút bấm: Nút màu xanh dương (User Button) nối ở PA0 */
#define BOARD_BUTTON_PORT     GPIOA_BASE
#define BOARD_BUTTON_PIN      0

/* 3. Cổng Console (Terminal): Dùng USART1 nối với máy tính */
#define BOARD_CONSOLE_UART    USART1_BASE

#endif /* BOARD_H */