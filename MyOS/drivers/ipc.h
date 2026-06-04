#ifndef IPC_H
#define IPC_H

#include <stdint.h>
#include "sync.h"

#define MAX_MESSAGE_COUNT 10

typedef struct {
    uint8_t *buffer;
    uint32_t length;
    uint32_t item_size;
    int head;
    int tail;
    os_mutex_t mutex_lock;
    os_sem_t sem_data;
    os_sem_t sem_space;
} os_msg_queue_t;

typedef struct {
    void *data;
    uint32_t size;
} os_msg_buffer_t;

os_status_t msg_queue_init(os_msg_queue_t *q, uint32_t length, uint32_t item_size);
void msg_queue_send(os_msg_queue_t *q, const void *item);
void msg_queue_send_to_back(os_msg_queue_t *q, const void *item);
void msg_queue_send_to_front(os_msg_queue_t *q, const void *item);
void msg_queue_receive(os_msg_queue_t *q, void *item);
os_status_t msg_queue_send_timeout(os_msg_queue_t *q, const void *item, uint32_t timeout_ticks);
os_status_t msg_queue_send_to_back_timeout(os_msg_queue_t *q, const void *item, uint32_t timeout_ticks);
os_status_t msg_queue_send_to_front_timeout(os_msg_queue_t *q, const void *item, uint32_t timeout_ticks);
os_status_t msg_queue_receive_timeout(os_msg_queue_t *q, void *item, uint32_t timeout_ticks);
os_status_t msg_queue_send_from_isr(os_msg_queue_t *q, const void *item);
os_status_t msg_queue_send_to_back_from_isr(os_msg_queue_t *q, const void *item);
os_status_t msg_queue_send_to_front_from_isr(os_msg_queue_t *q, const void *item);
os_status_t msg_queue_receive_from_isr(os_msg_queue_t *q, void *item);

os_status_t msg_buffer_alloc(os_msg_buffer_t *msg, uint32_t size);
void msg_buffer_release(os_msg_buffer_t *msg);
os_status_t msg_queue_send_buffer_timeout(os_msg_queue_t *q,
                                          os_msg_buffer_t *msg,
                                          uint32_t timeout_ticks);
os_status_t msg_queue_receive_buffer_timeout(os_msg_queue_t *q,
                                             os_msg_buffer_t *msg,
                                             uint32_t timeout_ticks);
os_status_t msg_queue_send_buffer_from_isr(os_msg_queue_t *q,
                                           os_msg_buffer_t *msg);
os_status_t msg_queue_receive_buffer_from_isr(os_msg_queue_t *q,
                                              os_msg_buffer_t *msg);

#endif /* IPC_H */
