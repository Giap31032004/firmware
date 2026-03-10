#include "heap.h"
#include "kernel.h"

static uint8_t heap_area[HEAP_SIZE] __attribute__((aligned(8))); // aligned(8) đảm bảo mảng này bắt đầu ở địa chỉ chia hết cho 8
static mem_block_t *free_list = NULL; // con trỏ đầu danh sách

void os_mem_init(void) {
    free_list = (mem_block_t *)heap_area;
    free_list->next = NULL;
    free_list->size = HEAP_SIZE - sizeof(mem_block_t);
    free_list->is_free = 1;
}

void* os_malloc(size_t size){
    void *ptr = NULL;
    size = (size + 7) & ~0x07; // Align size to 8 bytes

    OS_ENTER_CRITICAL();
    mem_block_t *current = free_list;
    while (current){
        if(current->is_free && current->size >= size){
            if(current->size > size + sizeof(mem_block_t) + 8){
                mem_block_t *new_block = (mem_block_t*)((uint8_t*)current + sizeof(mem_block_t) + size);
                new_block->size = current->size - size - sizeof(mem_block_t);
                new_block->is_free = 1;
                new_block->next = current->next;
                current->size = size;
                current->next = new_block;
            }
            current->is_free = 0;
            ptr = (void*)((uint8_t*)current + sizeof(mem_block_t));
            break;
        }
        current = current->next;
    }
    OS_EXIT_CRITICAL();
    return ptr;
}

void os_free(void *ptr) {
    if (ptr == NULL) return;

    OS_ENTER_CRITICAL();

    mem_block_t *block = (mem_block_t*)((uint8_t*)ptr - sizeof(mem_block_t));
    block->is_free = 1;

    if (block->next && block->next->is_free) {
        block->size += sizeof(mem_block_t) + block->next->size;
        block->next = block->next->next;
    }
    
    OS_EXIT_CRITICAL();
}