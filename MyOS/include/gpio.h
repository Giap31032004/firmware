#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>

/* ====================================================================
 * ĐỊNH NGHĨA MẶT NẠ CHÂN (PIN MASKS)
 * Phần này là logic C thuần túy, dùng cho mọi loại vi điều khiển
 * ==================================================================== */
#define GPIO_PIN_0          (1U << 0)
#define GPIO_PIN_1          (1U << 1)
#define GPIO_PIN_2          (1U << 2)
#define GPIO_PIN_3          (1U << 3)
#define GPIO_PIN_4          (1U << 4)
#define GPIO_PIN_5          (1U << 5)
#define GPIO_PIN_6          (1U << 6)
#define GPIO_PIN_7          (1U << 7)
#define GPIO_PIN_8          (1U << 8)
#define GPIO_PIN_9          (1U << 9)
#define GPIO_PIN_10         (1U << 10)
#define GPIO_PIN_11         (1U << 11)
#define GPIO_PIN_12         (1U << 12)
#define GPIO_PIN_13         (1U << 13)
#define GPIO_PIN_14         (1U << 14)
#define GPIO_PIN_15         (1U << 15)

/* --- ĐỊNH NGHĨA HƯỚNG (DIRECTION) --- */
#define GPIO_DIR_INPUT      0
#define GPIO_DIR_OUTPUT     1

/* ====================================================================
 * PUBLIC API (BẢN HỢP ĐỒNG GIAO TIẾP)
 * Các Task chỉ được phép gọi các hàm này, không cần biết ruột bên trong
 * ==================================================================== */

/* Lưu ý: Tham số port_base bây giờ sẽ nhận giá trị GPIOA_BASE, GPIOB_BASE 
   được định nghĩa trong memory_map.h thay vì cấu hình cứng ở đây */

void gpio_init(uint32_t port_base, uint16_t pin_mask, uint8_t direction);
void gpio_write(uint32_t port_base, uint16_t pin_mask, uint8_t value);
void gpio_toggle(uint32_t port_base, uint16_t pin_mask);
uint32_t gpio_read(uint32_t port_base, uint16_t pin_mask);

#endif /* GPIO_H */