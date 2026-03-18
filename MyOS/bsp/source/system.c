#include <stdint.h>

/* ========================================================================
 * 1. ĐỊNH NGHĨA THANH GHI VÀ MACRO
 * ======================================================================== */
#define FLASH_BASE      0x40023C00
#define FLASH_ACR       (*(volatile uint32_t *)(FLASH_BASE + 0x00))

#define RCC_BASE        0x40023800
#define RCC_CR          (*(volatile uint32_t *)(RCC_BASE + 0x00))
#define RCC_PLLCFGR     (*(volatile uint32_t *)(RCC_BASE + 0x04))
#define RCC_CFGR        (*(volatile uint32_t *)(RCC_BASE + 0x08))

#define SCB_BASE        0xE000ED00
#define SCB_VTOR        (*(volatile uint32_t *)(SCB_BASE + 0x08))
#define SCB_CPACR       (*(volatile uint32_t *)(SCB_BASE + 0x88))

/* Macro cho PLL để tránh Magic Numbers */
#define PLLP_DIV2       0U
#define PLLP_DIV4       1U
#define PLLP_DIV6       2U
#define PLLP_DIV8       3U

/* ========================================================================
 * 2. HÀM KHỞI TẠO HỆ THỐNG
 * ======================================================================== */
void SystemInit(void) {
    /* 1. Bật Bộ tính toán số thực FPU SỚM NHẤT */
    SCB_CPACR |= (0xF << 20);

    /* =========================================================
     * BƯỚC 1: ĐƯA HỆ THỐNG VỀ TRẠNG THÁI DEFAULT (HSI 16MHz)
     * ========================================================= */
    RCC_CR |= 0x00000001;                     /* Bật HSI */
    while (!(RCC_CR & (1 << 1)));             /* Chờ HSIRDY (HSI ổn định) */
    
    RCC_CFGR = 0x00000000;                    /* Chuyển nguồn SYSCLK về HSI */
    while ((RCC_CFGR & (3 << 2)) != 0x00);    /* Chờ xác nhận SWS = HSI */
    
    /* Lúc này đã an toàn để tắt các nguồn khác */
    RCC_CR &= 0xFEF6FFFF;                     /* Tắt HSE, CSS, PLL */
    RCC_PLLCFGR = 0x24003010;                 /* Reset cấu hình PLL */
    RCC_CR &= 0xFFFBFFFF;                     /* Reset HSEBYP */

    /* =========================================================
     * BƯỚC 2: CẤU HÌNH XUNG NHỊP 168MHz TỪ HSE (8MHz)
     * ========================================================= */
    uint32_t timeout;

    /* A. Bật Thạch anh ngoài (HSE) với Timeout */
    RCC_CR |= (1 << 16); 
    timeout = 50000;
    while (!(RCC_CR & (1 << 17)) && --timeout);
    if (timeout == 0) return; /* Lỗi HSE: Fallback chạy tiếp bằng HSI 16MHz */

    /* B. Cấu hình Bộ chia Bus */
    /* APB1 (/4) = 101b ở bit 10. APB2 (/2) = 100b ở bit 13 */
    RCC_CFGR |= (5 << 10) | (4 << 13);

    /* C. Thiết lập Hộp số PLL (8MHz / 8 * 336 / 2 = 168MHz) */
    uint32_t pll_m = 8;
    uint32_t pll_n = 336;
    uint32_t pll_p = PLLP_DIV2;
    uint32_t pll_q = 7;
    RCC_PLLCFGR = pll_m | (pll_n << 6) | (pll_p << 16) | (1 << 22) | (pll_q << 24);

    /* D. Bật PLL và chờ khóa (Lock) với Timeout */
    RCC_CR |= (1 << 24);
    timeout = 50000;
    while (!(RCC_CR & (1 << 25)) && --timeout);
    if (timeout == 0) return; /* Lỗi PLL: Fallback chạy tiếp bằng HSI 16MHz */

    /* =========================================================
     * BƯỚC 3: "VÍT GA" - KÍCH HOẠT TỐC ĐỘ CAO
     * ========================================================= */
    /* Bật Flash Latency (5 WS) và các Caches NGAY TRƯỚC KHI chuyển tốc độ */
    FLASH_ACR = (1 << 8) | (1 << 9) | (1 << 10) | 5;

    /* Chuyển SYSCLK sang xài mạch PLL (168MHz) */
    RCC_CFGR |= 2; 
    while ((RCC_CFGR & (3 << 2)) != (2 << 2)); /* Chờ SWS = PLL */

    /* =========================================================
     * BƯỚC 4: CHỐT BẢNG NGẶT
     * ========================================================= */
    SCB_VTOR = 0x08000000;
}