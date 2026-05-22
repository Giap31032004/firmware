#ifndef MYOS_KERNEL_HEAP_H
#define MYOS_KERNEL_HEAP_H

#include <stdint.h>
#include <stddef.h>

#define HEAP_SIZE (32 * 1024) // 32 KB heap size

typedef struct {
    uint32_t stack_base;
    uint32_t stack_size;
    uint32_t heap_base;
    uint32_t heap_size;
} memory_info_t;

/* Khởi tạo hệ thống quản lý bộ nhớ */
void memory_init(void);

/* Cấp phát và thu hồi */
void* os_malloc(size_t xWantedSize);
void  os_free(void *pv);

/* Các hàm đo lường sức khỏe RAM (Metrics) */
size_t get_free_heap_size(void);
size_t get_minimum_ever_free_heap_size(void);

#endif /* MYOS_KERNEL_HEAP_H */
