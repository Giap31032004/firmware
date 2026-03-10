#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h> //  for create size_t

#define HEAP_SIZE (32 * 1024) // 32 KB heap size

typedef struct mem_block {
    struct mem_block * next;
    size_t size;
    uint8_t is_free;
} mem_block_t;

void os_mem_init(void); // create heap
void* os_malloc(size_t size);
void os_free(void *ptr);

size_t os_get_free_heap_size(void);

#endif