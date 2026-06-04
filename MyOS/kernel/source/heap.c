#include "heap.h"
#include "kernel.h"

typedef struct BLOCK_LINK {
    struct BLOCK_LINK *next_free;
    size_t block_size;
} block_link_t;

#define BYTE_ALIGNMENT          8U
#define BYTE_ALIGNMENT_MASK     (BYTE_ALIGNMENT - 1U)
#define BLOCK_ALLOCATED_BIT     ((size_t)1U << ((sizeof(size_t) * 8U) - 1U))
#define BLOCK_SIZE_MASK         (~BLOCK_ALLOCATED_BIT)
#define MINIMUM_BLOCK_SIZE      ((size_t)(heap_struct_size * 2U))

extern uint32_t _end[];
extern uint32_t _estack;

static const size_t heap_struct_size =
    (sizeof(block_link_t) + BYTE_ALIGNMENT_MASK) & ~((size_t)BYTE_ALIGNMENT_MASK);

static block_link_t start;
static block_link_t *end_marker = NULL;
static uint8_t *heap_start_addr = NULL;
static uint8_t *heap_end_addr = NULL;

static size_t free_bytes_remaining = 0;
static size_t minimum_ever_free_bytes_remaining = 0;
static size_t largest_free_block = 0;
static size_t free_block_count = 0;

static int is_aligned_address(uintptr_t value)
{
    return (value & BYTE_ALIGNMENT_MASK) == 0U;
}

static size_t align_size(size_t size)
{
    if ((size & BYTE_ALIGNMENT_MASK) != 0U) {
        size += BYTE_ALIGNMENT - (size & BYTE_ALIGNMENT_MASK);
    }

    return size;
}

static void update_fragmentation_metrics(void)
{
    size_t largest = 0;
    size_t count = 0;

    for (block_link_t *block = start.next_free;
         block != NULL && block != end_marker;
         block = block->next_free) {
        size_t block_size = block->block_size & BLOCK_SIZE_MASK;
        count++;

        if (block_size > largest) {
            largest = block_size;
        }
    }

    largest_free_block = largest;
    free_block_count = count;
}

static void insert_block_into_free_list(block_link_t *block_to_insert)
{
    block_link_t *iterator;
    uint8_t *puc;

    for (iterator = &start;
         iterator->next_free < block_to_insert;
         iterator = iterator->next_free) {
    }

    puc = (uint8_t *)iterator;
    if ((puc + iterator->block_size) == (uint8_t *)block_to_insert) {
        iterator->block_size += block_to_insert->block_size;
        block_to_insert = iterator;
    }

    puc = (uint8_t *)block_to_insert;
    if ((puc + block_to_insert->block_size) == (uint8_t *)iterator->next_free) {
        if (iterator->next_free != end_marker) {
            block_to_insert->block_size += iterator->next_free->block_size;
            block_to_insert->next_free = iterator->next_free->next_free;
        } else {
            block_to_insert->next_free = end_marker;
        }
    } else {
        block_to_insert->next_free = iterator->next_free;
    }

    if (iterator != block_to_insert) {
        iterator->next_free = block_to_insert;
    }
}

static int heap_pointer_is_in_range(const void *ptr)
{
    const uint8_t *p = (const uint8_t *)ptr;

    return heap_start_addr != NULL &&
           heap_end_addr != NULL &&
           p >= (heap_start_addr + heap_struct_size) &&
           p < heap_end_addr;
}

static int block_header_is_sane(const block_link_t *block)
{
    size_t block_size;
    const uint8_t *block_start = (const uint8_t *)block;

    if (block == NULL ||
        (const uint8_t *)block < heap_start_addr ||
        (const uint8_t *)block >= heap_end_addr ||
        !is_aligned_address((uintptr_t)block)) {
        return 0;
    }

    block_size = block->block_size & BLOCK_SIZE_MASK;

    if (block_size < heap_struct_size ||
        (block_size & BYTE_ALIGNMENT_MASK) != 0U ||
        block_start + block_size > heap_end_addr) {
        return 0;
    }

    return 1;
}

static uint32_t calculate_fragmentation_percent(size_t free_bytes,
                                                size_t largest_block)
{
    if (free_bytes == 0U) {
        return 0U;
    }

    return (uint32_t)(100U - ((largest_block * 100U) / free_bytes));
}

void memory_init(void)
{
    block_link_t *first_free_block;
    uint8_t *heap_start = (uint8_t *)((uint32_t)&_end);
    uint8_t *heap_end = (uint8_t *)((uint32_t)&_estack - 4096U);
    size_t total_heap_size;

    if (((uint32_t)heap_start & BYTE_ALIGNMENT_MASK) != 0U) {
        heap_start += BYTE_ALIGNMENT - ((uint32_t)heap_start & BYTE_ALIGNMENT_MASK);
    }

    if (heap_end <= heap_start + heap_struct_size) {
        kernel_panic("heap range invalid", __FILE__, __LINE__);
    }

    total_heap_size = (size_t)(heap_end - heap_start);
    total_heap_size &= ~((size_t)BYTE_ALIGNMENT_MASK);

    heap_start_addr = heap_start;
    heap_end_addr = heap_start + total_heap_size;

    start.next_free = (void *)heap_start_addr;
    start.block_size = 0U;

    end_marker = (void *)(heap_end_addr - heap_struct_size);
    end_marker->block_size = 0U;
    end_marker->next_free = NULL;

    first_free_block = (void *)heap_start_addr;
    first_free_block->block_size = total_heap_size - heap_struct_size;
    first_free_block->next_free = end_marker;

    free_bytes_remaining = first_free_block->block_size;
    minimum_ever_free_bytes_remaining = first_free_block->block_size;
    update_fragmentation_metrics();
}

void *os_malloc(size_t wanted_size)
{
    block_link_t *block;
    block_link_t *previous_block;
    block_link_t *new_block;
    void *ret = NULL;

    if (wanted_size == 0U) {
        return NULL;
    }

    if (wanted_size > (BLOCK_ALLOCATED_BIT - heap_struct_size - BYTE_ALIGNMENT)) {
        return NULL;
    }

    wanted_size += heap_struct_size;
    wanted_size = align_size(wanted_size);

    if (wanted_size == 0U || (wanted_size & BLOCK_ALLOCATED_BIT) != 0U) {
        return NULL;
    }

    OS_ENTER_CRITICAL();
    {
        if (wanted_size <= free_bytes_remaining) {
            previous_block = &start;
            block = start.next_free;

            while (block != end_marker &&
                   block->block_size < wanted_size &&
                   block->next_free != NULL) {
                previous_block = block;
                block = block->next_free;
            }

            if (block != end_marker && block->block_size >= wanted_size) {
                ret = (void *)(((uint8_t *)block) + heap_struct_size);
                previous_block->next_free = block->next_free;

                if ((block->block_size - wanted_size) > MINIMUM_BLOCK_SIZE) {
                    new_block = (void *)(((uint8_t *)block) + wanted_size);
                    new_block->block_size = block->block_size - wanted_size;
                    block->block_size = wanted_size;
                    insert_block_into_free_list(new_block);
                }

                free_bytes_remaining -= block->block_size;
                if (free_bytes_remaining < minimum_ever_free_bytes_remaining) {
                    minimum_ever_free_bytes_remaining = free_bytes_remaining;
                }

                block->block_size |= BLOCK_ALLOCATED_BIT;
                block->next_free = NULL;
                update_fragmentation_metrics();
            }
        }
    }
    OS_EXIT_CRITICAL();

    return ret;
}

void os_free(void *ptr)
{
    uint8_t *payload = (uint8_t *)ptr;
    block_link_t *block;
    size_t block_size;

    if (ptr == NULL) {
        return;
    }

    if (!heap_pointer_is_in_range(ptr) ||
        !is_aligned_address((uintptr_t)ptr)) {
        kernel_panic("heap free invalid pointer", __FILE__, __LINE__);
    }

    block = (void *)(payload - heap_struct_size);

    OS_ENTER_CRITICAL();
    {
        if (!block_header_is_sane(block)) {
            kernel_panic("heap free corrupt header", __FILE__, __LINE__);
        }

        if ((block->block_size & BLOCK_ALLOCATED_BIT) == 0U) {
            kernel_panic("heap double free", __FILE__, __LINE__);
        }

        if (block->next_free != NULL) {
            kernel_panic("heap allocated block corrupt", __FILE__, __LINE__);
        }

        block->block_size &= BLOCK_SIZE_MASK;
        block_size = block->block_size;

        if (block_size > (BLOCK_ALLOCATED_BIT - free_bytes_remaining)) {
            kernel_panic("heap free accounting overflow", __FILE__, __LINE__);
        }

        free_bytes_remaining += block_size;
        insert_block_into_free_list(block);
        update_fragmentation_metrics();
    }
    OS_EXIT_CRITICAL();
}

size_t os_get_free_heap_size(void)
{
    return free_bytes_remaining;
}

size_t os_get_minimum_ever_free_heap_size(void)
{
    return minimum_ever_free_bytes_remaining;
}

size_t os_get_largest_free_block(void)
{
    return largest_free_block;
}

uint32_t os_get_heap_fragmentation_percent(void)
{
    return calculate_fragmentation_percent(free_bytes_remaining,
                                           largest_free_block);
}

void os_get_heap_stats(os_heap_stats_t *stats)
{
    if (stats == NULL) {
        return;
    }

    OS_ENTER_CRITICAL();
    {
        stats->free_bytes = free_bytes_remaining;
        stats->minimum_ever_free_bytes = minimum_ever_free_bytes_remaining;
        stats->largest_free_block = largest_free_block;
        stats->free_block_count = free_block_count;
        stats->fragmentation_percent =
            calculate_fragmentation_percent(free_bytes_remaining,
                                            largest_free_block);
    }
    OS_EXIT_CRITICAL();
}

size_t get_free_heap_size(void)
{
    return os_get_free_heap_size();
}

size_t get_minimum_ever_free_heap_size(void)
{
    return os_get_minimum_ever_free_heap_size();
}
