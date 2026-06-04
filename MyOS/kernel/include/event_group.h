#ifndef MYOS_KERNEL_EVENT_GROUP_H
#define MYOS_KERNEL_EVENT_GROUP_H

#include <stdint.h>

#include "kernel.h"
#include "queue.h"

typedef uint32_t os_event_bits_t;

typedef struct {
    os_event_bits_t bits;
    queue_t wait_list;
} os_event_group_t;

void event_group_init(os_event_group_t *event_group);
os_event_bits_t event_group_get_bits(os_event_group_t *event_group);
os_event_bits_t event_group_set_bits(os_event_group_t *event_group,
                                     os_event_bits_t bits_to_set);
os_event_bits_t event_group_set_bits_from_isr(os_event_group_t *event_group,
                                              os_event_bits_t bits_to_set);
os_event_bits_t event_group_clear_bits(os_event_group_t *event_group,
                                       os_event_bits_t bits_to_clear);
os_event_bits_t event_group_wait_bits(os_event_group_t *event_group,
                                      os_event_bits_t bits_to_wait_for,
                                      int clear_on_exit,
                                      int wait_for_all,
                                      uint32_t timeout_ticks);

#endif /* MYOS_KERNEL_EVENT_GROUP_H */
