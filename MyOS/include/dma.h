#ifndef DMA_H
#define DMA_H

#include <stdint.h>
#include <stdbool.h>

/* --- CẤU TRÚC BẢNG ĐIỀU KHIỂN --- */
typedef struct {
    volatile void *src_end_ptr;
    volatile void *dst_end_ptr;
    volatile uint32_t control;
    volatile uint32_t unused;
} dma_control_entry_t;

/* --- PUBLIC FUNCTIONS --- */
void dma_init(void);
bool dma_memcpy(void *src, void *dst, uint32_t size);

#endif // DMA_H