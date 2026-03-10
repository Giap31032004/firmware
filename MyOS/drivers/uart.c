#include "uart.h"
#include "kernel.h"
#include <stdio.h> 

#define UART0_BASE      0x4000C000

#define UART0_DR        (*(volatile uint32_t *)(UART0_BASE + 0x000)) // Data Register
#define UART0_FR        (*(volatile uint32_t *)(UART0_BASE + 0x018)) // Flag Register
#define UART0_IBRD      (*(volatile uint32_t *)(UART0_BASE + 0x024)) // Integer Baud Rate Divisor
#define UART0_FBRD      (*(volatile uint32_t *)(UART0_BASE + 0x028)) // Fractional Baud Rate Divisor
#define UART0_LCRH      (*(volatile uint32_t *)(UART0_BASE + 0x02C)) // Line Control
#define UART0_CTL       (*(volatile uint32_t *)(UART0_BASE + 0x030)) // Control
#define UART0_IM        (*(volatile uint32_t *)(UART0_BASE + 0x038)) // Interrupt Mask
#define UART0_ICR       (*(volatile uint32_t *)(UART0_BASE + 0x044)) // Interrupt Clear

// Cờ trạng thái (Flags)
#define UART_FR_RXFE    (1 << 4) // FIFO Nhận đang rỗng
#define UART_FR_TXFF    (1 << 5) // FIFO Gửi đang đầy
#define UART_RXIM       (1 << 4) // Bit cho phép ngắt nhận (RX Interrupt Mask)

// NVIC (Quản lý ngắt ngoại vi)
#define NVIC_EN0        (*(volatile uint32_t *)0xE000E100) // Enable IRQ 0-31

#define RX_BUFFER_SIZE 128
static volatile char rx_buffer[RX_BUFFER_SIZE];
static volatile int rx_head = 0;
static volatile int rx_tail = 0;

// Semaphore: Báo hiệu có dữ liệu mới -> Đánh thức Task đang gọi uart_getc
os_sem_t uart_rx_semaphore;

// Mutex: Khóa quyền in -> Chỉ 1 Task được in tại 1 thời điểm
os_mutex_t uart_tx_mutex; 

/* =============================================================
   HÀM KHỞI TẠO
   ============================================================= */
void uart_init(void) {
    sem_init(&uart_rx_semaphore, 0); 
    mutex_init(&uart_tx_mutex);      

    /* 2. Tắt UART trước khi cấu hình */
    UART0_CTL &= ~0x01; // Clear bit 0 (UARTEN)

    UART0_IBRD = 27; 
    UART0_FBRD = 8;

    UART0_LCRH = (0x3 << 5) | (1 << 4); 
    UART0_IM |= UART_RXIM;  
    UART0_CTL |= (1 << 0) | (1 << 8) | (1 << 9); 
    NVIC_EN0 |= (1 << 5); 
}

/* =============================================================
   HÀM NỘI BỘ (INTERNAL) - KHÔNG KHÓA MUTEX
   ============================================================= */
void uart_putc_raw(char c) {
    while (UART0_FR & UART_FR_TXFF); 
    UART0_DR = c;
}

/* =============================================================
   HÀM PUBLIC (API)
   ============================================================= */
void uart_putc(char c) {
    mutex_lock(&uart_tx_mutex);
    uart_putc_raw(c);
    mutex_unlock(&uart_tx_mutex);
}

// In chuỗi an toàn (Thread-safe)
void uart_print(const char *s) {
    mutex_lock(&uart_tx_mutex);
    
    while (*s) {
        if (*s == '\n') uart_putc_raw('\r');
        uart_putc_raw(*s++);
    }
    
    mutex_unlock(&uart_tx_mutex); 
}

// Nhận ký tự (Blocking)
char uart_getc(void) {
    sem_wait(&uart_rx_semaphore);
    
    OS_ENTER_CRITICAL(); 
    char c = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;
    OS_EXIT_CRITICAL();

    return c;
}

/* =============================================================
   INTERRUPT SERVICE ROUTINE (ISR)
   ============================================================= */
void UART0_Handler(void) {
    // 1. Xóa cờ ngắt (Bắt buộc)
    UART0_ICR |= UART_RXIM;

    // 2. Đọc hết FIFO phần cứng 
    while((UART0_FR & UART_FR_RXFE) == 0) {
        char c = (char)(UART0_DR & 0xFF);
        
        // Ghi vào Ring Buffer
        int next_head = (rx_head + 1) % RX_BUFFER_SIZE;
        if (next_head != rx_tail) { 
            rx_buffer[rx_head] = c;
            rx_head = next_head;
            
            sem_signal(&uart_rx_semaphore);
        } else {
            // Buffer tràn: Có thể bỏ qua hoặc log lỗi
        }
    }
}

/* =============================================================
   PRINTF INTEGRATION (HOOK)
   Hàm này giúp printf("%d", 123) hoạt động!
   ============================================================= */
int _write(int file, char *ptr, int len) {
    mutex_lock(&uart_tx_mutex); // Khóa 1 lần cho cả chuỗi dài -> Hiệu suất cao
    
    for (int i = 0; i < len; i++) {
        if (ptr[i] == '\n') uart_putc_raw('\r');
        uart_putc_raw(ptr[i]);
    }
    
    mutex_unlock(&uart_tx_mutex);
    return len;
}

/* =============================================================
   LEGACY SUPPORT FUNCTIONS
   ============================================================= */

// Helper: Chuyển 4 bit (0-15) thành ký tự Hex
static char nibble_to_hex(uint8_t nibble) {
    return (nibble < 10) ? ('0' + nibble) : ('A' + (nibble - 10));
}

void uart_print_hex(uint8_t n) {
    char str[3];
    str[0] = nibble_to_hex((n >> 4) & 0x0F);
    str[1] = nibble_to_hex(n & 0x0F);
    str[2] = '\0';
    uart_print(str); // Tái sử dụng hàm thread-safe
}

void uart_print_dec(uint32_t val) {
    char buf[12];
    int i = 0;
    if (val == 0) {
        uart_putc('0');
        return;
    }
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    // Đảo ngược chuỗi để in
    mutex_lock(&uart_tx_mutex);
    while (i > 0) uart_putc_raw(buf[--i]);
    mutex_unlock(&uart_tx_mutex);
}

/* Hàm in số 32-bit dạng Hex (0x1234ABCD) - SỬ DỤNG LẠI nibble_to_hex */
void uart_print_hex32(uint32_t n) {
    char str[11];
    str[0] = '0';
    str[1] = 'x';
    
    // In từ byte cao nhất (MSB) xuống thấp nhất
    for (int i = 0; i < 8; i++) {
        // Dịch để lấy từng cụm 4 bit (nibble), bắt đầu từ bit 28
        uint8_t nibble = (n >> (28 - (i * 4))) & 0x0F;
        str[2 + i] = nibble_to_hex(nibble); // Sử dụng lại hàm ở trên
    }
    str[10] = '\0';
    
    // Dùng hàm in an toàn thread-safe
    uart_print(str);
}

