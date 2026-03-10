#include <stdint.h>

/* ====================================================================
 * ĐỊNH NGHĨA THANH GHI SYSTEM CONTROL (Dành riêng cho LM3S6965)
 * ==================================================================== */
#define SYSCTL_BASE         0x400FE000
#define SYSCTL_RIS          (*(volatile uint32_t *)(SYSCTL_BASE + 0x050))
#define SYSCTL_RCC          (*(volatile uint32_t *)(SYSCTL_BASE + 0x060))

/* Các Bit quan trọng trong thanh ghi RCC */
#define RCC_BYPASS          (1 << 11)  
#define RCC_PWRDN           (1 << 13)  
#define RCC_USESYSDIV       (1 << 22)  

/* Các giá trị cấu hình */
#define RCC_XTAL_8MHZ       (0x0E << 6)  // Thạch anh 8MHz
#define RCC_OSCSRC_MAIN     (0x00 << 4)  // Dùng Main Oscillator
#define RCC_SYSDIV_50MHZ    (0x03 << 23) // Chia (3+1)=4. 200MHz / 4 = 50MHz

#define RIS_PLLLRIS         (1 << 6)     // Cờ báo PLL đã khóa

/* ====================================================================
 * HÀM KHỞI TẠO XUNG NHỊP (Được gọi từ startup.s)
 * ==================================================================== */
void SystemInit(void) {
    /* 1. Bật chế độ BYPASS (chạy tạm bằng thạch anh gốc để cấu hình an toàn) */
    SYSCTL_RCC |= RCC_BYPASS;

    /* 2. Xóa các cấu hình chia tần số cũ */
    SYSCTL_RCC &= ~(0x07C007F0);

    /* 3. Nạp cấu hình mới: 8MHz XTAL, Main OSC, bật bộ chia 50MHz */
    SYSCTL_RCC |= (RCC_XTAL_8MHZ | RCC_OSCSRC_MAIN | RCC_SYSDIV_50MHZ | RCC_USESYSDIV);

    /* 4. Bật nguồn cho mạch PLL */
    SYSCTL_RCC &= ~RCC_PWRDN;

    /* 5. Chờ mạch PLL khởi động và khóa tần số chuẩn (Lock) */
    while ((SYSCTL_RIS & RIS_PLLLRIS) == 0) {
        // Busy wait
    }

    /* 6. Tắt BYPASS, chính thức cấp xung nhịp 50MHz cho hệ thống */
    SYSCTL_RCC &= ~RCC_BYPASS;
}