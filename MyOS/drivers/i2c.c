#include "i2c.h"
#include "gpio.h"       // Chứa GPIO_PORTB_BASE, SYSCTL_RCGC2...
#include "kernel.h"     // Chứa OS_ENTER/EXIT_CRITICAL
#include "sync.h"       // Chứa os_mutex_t, mutex_lock, mutex_unlock

/* --- ĐỊNH NGHĨA ĐỊA CHỈ BASE (LM3S6965) --- */
#define I2C_MASTER_BASE     0x40020000

/* --- THANH GHI I2C --- */
#define I2C_MSA    (*(volatile uint32_t *)(I2C_MASTER_BASE + 0x000)) // Master Slave Address
#define I2C_MCS    (*(volatile uint32_t *)(I2C_MASTER_BASE + 0x004)) // Master Control/Status
#define I2C_MDR    (*(volatile uint32_t *)(I2C_MASTER_BASE + 0x008)) // Master Data
#define I2C_MTPR   (*(volatile uint32_t *)(I2C_MASTER_BASE + 0x00C)) // Master Timer Period
#define I2C_MCR    (*(volatile uint32_t *)(I2C_MASTER_BASE + 0x020)) // Master Configuration

/* --- THANH GHI HỆ THỐNG (Bổ sung cho I2C) --- */
/* SYSCTL_RCGC1: Legacy Clock Control cho I2C */
#define SYSCTL_RCGC1    (*(volatile uint32_t *)0x400FE104) 

/* --- GPIO OFFSETS (Dùng lại base từ gpio.h) --- */
/* Lưu ý: Các offset này nên được define trong gpio.h, nhưng define cục bộ ở đây cũng ổn */
#define GPIO_AFSEL_OFFSET   0x420
#define GPIO_ODR_OFFSET     0x50C
#define GPIO_DEN_OFFSET     0x51C

/* --- CỜ ĐIỀU KHIỂN --- */
#define CMD_RUN       0x01
#define CMD_START     0x02
#define CMD_STOP      0x04
#define CMD_ACK       0x08

/* --- CỜ TRẠNG THÁI --- */
#define STS_BUSY      0x01
#define STS_ERROR     0x02

/* --- MUTEX BẢO VỆ BUS --- */
static os_mutex_t i2c_mutex;

/* --- HÀM PHỤ TRỢ: CHỜ BUS RẢNH --- */
static bool i2c_wait_busy(void) {
    int timeout = 100000;
    while (I2C_MCS & STS_BUSY) {
        if (--timeout == 0) return false; // Timeout
    }
    if (I2C_MCS & STS_ERROR) return false; // Bus Error
    return true;
}

/* --- KHỞI TẠO --- */
void i2c_init(void) {
    /* 1. Init Mutex */
    mutex_init(&i2c_mutex);

    OS_ENTER_CRITICAL();

    /* 2. Cấp Clock */
    SYSCTL_RCGC1 |= (1 << 12); // Enable I2C0 (Bit 12)
    
    // Bật Clock cho Port B (Bit 1 của RCGC2)
    // Dùng macro hoặc truy cập trực tiếp (phụ thuộc vào gpio.h của bạn)
    // Cách an toàn nhất là truy cập trực tiếp nếu gpio.h không expose SYSCTL_RCGC2
    (*(volatile uint32_t *)0x400FE108) |= (1 << 1); 

    // Delay nhỏ để clock ổn định
    volatile int i; for(i=0; i<100; i++);

    /* 3. Cấu hình GPIO PB2 (SCL) và PB3 (SDA) */
    volatile uint32_t *gpio_afsel = (uint32_t *)(GPIO_PORTB_BASE + GPIO_AFSEL_OFFSET);
    volatile uint32_t *gpio_odr   = (uint32_t *)(GPIO_PORTB_BASE + GPIO_ODR_OFFSET);
    volatile uint32_t *gpio_den   = (uint32_t *)(GPIO_PORTB_BASE + GPIO_DEN_OFFSET);

    *gpio_afsel |= (1<<2) | (1<<3); // Chức năng thay thế
    *gpio_odr   |= (1<<3);          // SDA (PB3) phải là Open-Drain
    *gpio_den   |= (1<<2) | (1<<3); // Digital Enable

    /* 4. Khởi tạo I2C Master */
    I2C_MCR = 0x10; // Enable Master function

    /* 5. Cài đặt tốc độ (Standard Mode 100kbps) */
    I2C_MTPR = 0x18; // Giả định System Clock 50MHz

    OS_EXIT_CRITICAL();
}

/* --- GHI 1 BYTE --- */
bool i2c_write_byte(uint8_t slave_addr, uint8_t reg_addr, uint8_t data) {
    mutex_lock(&i2c_mutex);

    // Gửi địa chỉ thanh ghi (Write mode)
    I2C_MSA = (slave_addr << 1) | 0;
    I2C_MDR = reg_addr;
    I2C_MCS = CMD_START | CMD_RUN;
    
    if (!i2c_wait_busy()) { 
        mutex_unlock(&i2c_mutex); 
        return false; 
    }

    // Gửi dữ liệu
    I2C_MDR = data;
    I2C_MCS = CMD_RUN | CMD_STOP;
    
    if (!i2c_wait_busy()) { 
        mutex_unlock(&i2c_mutex); 
        return false; 
    }

    mutex_unlock(&i2c_mutex);
    return true;
}

/* --- ĐỌC 1 BYTE --- */
bool i2c_read_byte(uint8_t slave_addr, uint8_t reg_addr, uint8_t *data) {
    mutex_lock(&i2c_mutex);

    // Ghi dummy (chọn thanh ghi cần đọc)
    I2C_MSA = (slave_addr << 1) | 0;
    I2C_MDR = reg_addr;
    I2C_MCS = CMD_START | CMD_RUN;
    
    if (!i2c_wait_busy()) { 
        mutex_unlock(&i2c_mutex); 
        return false; 
    }

    // Đọc dữ liệu
    I2C_MSA = (slave_addr << 1) | 1; // Read mode
    I2C_MCS = CMD_START | CMD_RUN | CMD_STOP | CMD_ACK;
    
    if (!i2c_wait_busy()) { 
        mutex_unlock(&i2c_mutex); 
        return false; 
    }

    *data = (uint8_t)I2C_MDR;
    
    mutex_unlock(&i2c_mutex);
    return true;
}