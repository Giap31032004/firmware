#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include <stdbool.h>

/* --- PUBLIC FUNCTIONS --- */

/**
 * @brief Khởi tạo module I2C0 và GPIO tương ứng (PB2, PB3)
 */
void i2c_init(void);

/**
 * @brief Ghi 1 byte vào thanh ghi của thiết bị Slave
 * @param slave_addr: Địa chỉ 7-bit của thiết bị Slave
 * @param reg_addr: Địa chỉ thanh ghi bên trong Slave cần ghi
 * @param data: Dữ liệu cần ghi
 * @return true nếu thành công, false nếu lỗi/timeout
 */
bool i2c_write_byte(uint8_t slave_addr, uint8_t reg_addr, uint8_t data);

/**
 * @brief Đọc 1 byte từ thanh ghi của thiết bị Slave
 * @param slave_addr: Địa chỉ 7-bit của thiết bị Slave
 * @param reg_addr: Địa chỉ thanh ghi muốn đọc
 * @param data: Con trỏ để lưu dữ liệu đọc được
 * @return true nếu thành công
 */
bool i2c_read_byte(uint8_t slave_addr, uint8_t reg_addr, uint8_t *data);

#endif