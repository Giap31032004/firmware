#include "i2c.h"
#include "kernel.h"     // Chứa os_delay 
#include "sync.h"       // Chứa os_mutex_t
#include <stddef.h>     // Chứa NULL

/* --- MUTEX BẢO VỆ BUS --- */
static os_mutex_t i2c_mutex;

/* --- KHỞI TẠO (STUB CHO STM32F407) --- */
void i2c_init(void) {
    mutex_init(&i2c_mutex);
}

/* --- GHI 1 BYTE (STUB) --- */
bool i2c_write_byte(uint8_t slave_addr, uint8_t reg_addr, uint8_t data) {
    mutex_lock(&i2c_mutex);
    os_delay(2); 
    mutex_unlock(&i2c_mutex);
    return true; 
}

/* --- ĐỌC 1 BYTE (STUB) --- */
bool i2c_read_byte(uint8_t slave_addr, uint8_t reg_addr, uint8_t *data) {
    mutex_lock(&i2c_mutex);
    os_delay(2); 
    if (data != NULL) {
        *data = 25;  /* Trả về nhiệt độ giả 25 độ C */
    }
    mutex_unlock(&i2c_mutex);
    return true;
}