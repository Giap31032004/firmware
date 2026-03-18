#include "uart.h"
#include "kernel.h"
#include "memory_map.h" /* BẮT BUỘC: Đã chứa định nghĩa GPIOA và USART1 */
#include <stdio.h> 

/* Các bit trạng thái của USART1 (STM32F4) */
#define USART_SR_RXNE   (1 << 5) /* Cờ: Thanh ghi nhận (RX) có dữ liệu (Not Empty) */
#define USART_SR_TXE    (1 << 7) /* Cờ: Thanh ghi gửi (TX) trống (Empty) -> Sẵn sàng gửi */
#define USART_CR1_RXNEIE (1 << 5) /* Cờ: Bật ngắt khi nhận được dữ liệu (RXNE Interrupt Enable) */

/* NVIC cho STM32F4 (Interrupt Controller) */
#define NVIC_ISER1      (*(volatile uint32_t *)0xE000E104) /* Thanh ghi bật ngắt từ IRQ32 đến IRQ63 */

#define RX_BUFFER_SIZE 128
static volatile char rx_buffer[RX_BUFFER_SIZE];
static volatile int rx_head = 0;
static volatile int rx_tail = 0;

/* Semaphore & Mutex cho đa nhiệm */
os_sem_t uart_rx_semaphore;
os_mutex_t uart_tx_mutex; 

/* =============================================================
   HÀM KHỞI TẠO (STM32F407 - USART1 ở tốc độ 115200)
   ============================================================= */
void uart_init(void) {
    /* Khởi tạo OS Sync Objects */
    sem_init(&uart_rx_semaphore, 0); 
    mutex_init(&uart_tx_mutex);      

    /* 1. Bật nguồn (Clock) cho GPIOA và USART1 */
    RCC->AHB1ENR |= (1 << 0);   /* Bit 0: GPIOA EN */
    RCC->APB2ENR |= (1 << 4);   /* Bit 4: USART1 EN */

    /* 2. Cấu hình chân PA9 (TX) và PA10 (RX) sang chế độ Alternate Function */
    GPIOA->MODER &= ~((3 << (9 * 2)) | (3 << (10 * 2))); /* Xóa cấu hình cũ */
    GPIOA->MODER |=  ((2 << (9 * 2)) | (2 << (10 * 2))); /* Đặt thành 10 (AF mode) */

    /* Chọn AF7 cho PA9 và PA10 (AF7 là chức năng USART1-3) */
    GPIOA->AFR[1] &= ~((0xF << (1 * 4)) | (0xF << (2 * 4))); /* Xóa AF cũ của Pin 9 (AFR[1] bit 4-7) và Pin 10 (bit 8-11) */
    GPIOA->AFR[1] |=  ((7 << (1 * 4)) | (7 << (2 * 4)));     /* Đặt thành AF7 */

    /* 3. Cấu hình USART1 */
    USART1->CR1 = 0x00; /* Tắt USART1 đi để cấu hình cho an toàn */

    /* Tốc độ Baud: Xung nhịp mặc định HSI là 16MHz. Để đạt 115200 baud -> BRR = 0x8A */
    USART1->BRR = 0x008A;
    
    /* Bật UART (UE, bit 13), Bật bộ phát (TE, bit 3), Bật bộ thu (RE, bit 2) */
    USART1->CR1 |= (1 << 13) | (1 << 3) | (1 << 2); 

    /* 4. Cấu hình Ngắt (Interrupt) cho việc nhận dữ liệu */
    USART1->CR1 |= USART_CR1_RXNEIE; /* Cho phép ngắt khi cờ RXNE bật lên */

    /* Bật ngắt USART1 trong NVIC. USART1 có IRQ Number là 37 (nằm ở thanh ghi ISER1, bit 5) */
    NVIC_ISER1 |= (1 << (37 - 32)); 
}

/* =============================================================
   HÀM NỘI BỘ (INTERNAL) - KHÔNG KHÓA MUTEX
   ============================================================= */
void uart_putc_raw(char c) {
    /* Chờ cho đến khi cờ TXE (Transmit Data Register Empty) bật lên mức 1 */
    while (!(USART1->SR & USART_SR_TXE)); 
    /* Đẩy dữ liệu vào thanh ghi Data Register */
    USART1->DR = c;
}

/* =============================================================
   HÀM PUBLIC (API) - GIỮ NGUYÊN HOÀN TOÀN LOGIC CŨ!
   ============================================================= */
void uart_putc(char c) {
    mutex_lock(&uart_tx_mutex);
    uart_putc_raw(c);
    mutex_unlock(&uart_tx_mutex);
}

void uart_print(const char *s) {
    mutex_lock(&uart_tx_mutex);
    while (*s) {
        if (*s == '\n') uart_putc_raw('\r');
        uart_putc_raw(*s++);
    }
    mutex_unlock(&uart_tx_mutex); 
}

char uart_getc(void) {
    sem_wait(&uart_rx_semaphore);
    
    OS_ENTER_CRITICAL(); 
    char c = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;
    OS_EXIT_CRITICAL();

    return c;
}

/* =============================================================
   INTERRUPT SERVICE ROUTINE (ISR) - DÀNH CHO STM32F4
   ============================================================= */
/* Lưu ý: Tên hàm này PHẢI khớp chính xác với tên trong file startup.s của bạn! */
void USART1_Handler(void) {
    /* Kiểm tra xem ngắt này có phải do "Có dữ liệu đến" (RXNE) sinh ra không */
    if (USART1->SR & USART_SR_RXNE) {
        /* Đọc dữ liệu ra từ Data Register (Việc đọc này sẽ tự động xóa cờ ngắt RXNE) */
        char c = (char)(USART1->DR & 0xFF);
        
        /* Ghi vào Ring Buffer */
        int next_head = (rx_head + 1) % RX_BUFFER_SIZE;
        if (next_head != rx_tail) { 
            rx_buffer[rx_head] = c;
            rx_head = next_head;
            
            /* Đánh thức Task đang chờ lệnh uart_getc() */
            sem_signal(&uart_rx_semaphore);
        }
    }
}

/* =============================================================
   PRINTF INTEGRATION (HOOK) & LEGACY FUNCTIONS
   (Phần này logic C thuần túy, KHÔNG CHẠM VÀO PHẦN CỨNG NÊN GIỮ NGUYÊN 100%)
   ============================================================= */
int _write(int file, char *ptr, int len) {
    mutex_lock(&uart_tx_mutex); 
    for (int i = 0; i < len; i++) {
        if (ptr[i] == '\n') uart_putc_raw('\r');
        uart_putc_raw(ptr[i]);
    }
    mutex_unlock(&uart_tx_mutex);
    return len;
}

static char nibble_to_hex(uint8_t nibble) {
    return (nibble < 10) ? ('0' + nibble) : ('A' + (nibble - 10));
}

void uart_print_hex(uint8_t n) {
    char str[3];
    str[0] = nibble_to_hex((n >> 4) & 0x0F);
    str[1] = nibble_to_hex(n & 0x0F);
    str[2] = '\0';
    uart_print(str); 
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
    mutex_lock(&uart_tx_mutex);
    while (i > 0) uart_putc_raw(buf[--i]);
    mutex_unlock(&uart_tx_mutex);
}

void uart_print_hex32(uint32_t n) {
    char str[11];
    str[0] = '0';
    str[1] = 'x';
    for (int i = 0; i < 8; i++) {
        uint8_t nibble = (n >> (28 - (i * 4))) & 0x0F;
        str[2 + i] = nibble_to_hex(nibble);
    }
    str[10] = '\0';
    uart_print(str);
}