#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

#define HEAP_SIZE (32 * 1024) // 32 KB heap size

/* Khởi tạo hệ thống quản lý bộ nhớ */
void os_mem_init(void);

/* Cấp phát và thu hồi */
void* os_malloc(size_t xWantedSize);
void  os_free(void *pv);

/* Các hàm đo lường sức khỏe RAM (Metrics) */
size_t os_get_free_heap_size(void);
size_t os_get_minimum_ever_free_heap_size(void);

#endif /* MEMORY_H */