#include "dma.h"
#include "kernel.h" 

/* Đã xóa toàn bộ #define phần cứng cũ của LM3S6965 */

void dma_init(void) {
    /* TODO: Viết Driver DMA cho STM32F407 sau */
    return;
}

bool dma_memcpy(void *src, void *dst, uint32_t size) {
    /* TODO: Viết Driver DMA cho STM32F407 sau */
    
    /* Tạm thời dùng CPU copy chay (memcpy) để hệ thống vẫn chạy được 
       mà không cần phần cứng DMA thật */
    if (size == 0) return false;
    
    char *s = (char *)src;
    char *d = (char *)dst;
    for (uint32_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
    
    return true;
}