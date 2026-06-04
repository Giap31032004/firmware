#ifndef MYOS_KERNEL_HEAP_H
#define MYOS_KERNEL_HEAP_H

#include <stdint.h>
#include <stddef.h>

#define HEAP_SIZE (32 * 1024)

typedef struct {
    uint32_t stack_alloc_base;
    uint32_t stack_base;
    uint32_t stack_size;
    uint32_t heap_base;
    uint32_t heap_size;
} memory_info_t;

typedef struct {
    size_t free_bytes;
    size_t minimum_ever_free_bytes;
    size_t largest_free_block;
    size_t free_block_count;
    uint32_t fragmentation_percent;
} os_heap_stats_t;

void memory_init(void);

void *os_malloc(size_t xWantedSize);
void os_free(void *pv);

size_t os_get_free_heap_size(void);
size_t os_get_minimum_ever_free_heap_size(void);
size_t os_get_largest_free_block(void);
uint32_t os_get_heap_fragmentation_percent(void);
void os_get_heap_stats(os_heap_stats_t *stats);

size_t get_free_heap_size(void);
size_t get_minimum_ever_free_heap_size(void);

#endif /* MYOS_KERNEL_HEAP_H */
