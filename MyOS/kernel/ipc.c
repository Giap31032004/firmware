#include "ipc.h"
#include <stdint.h>

void msg_queue_init(os_msg_queue_t *q){
    q->head = 0;
    q->tail = 0;

    mutex_init(&q->mutex_lock); 
    sem_init(&q->sem_data, 0);
    sem_init(&q->sem_space, MAX_MESSAGE_COUNT); 
}

void msg_queue_send(os_msg_queue_t *q, int32_t data){
    sem_wait(&q->sem_space); 

    mutex_lock(&q->mutex_lock); 

    q->buffer[q->head] = data;
    q->head = (q->head + 1) % MAX_MESSAGE_COUNT;

    mutex_unlock(&q->mutex_lock); 

    sem_signal(&q->sem_data); 
}

int32_t msg_queue_receive(os_msg_queue_t *q){
    int32_t data;
    sem_wait(&q->sem_data); 

    mutex_lock(&q->mutex_lock); 

    data = q->buffer[q->tail];
    q->tail = (q->tail + 1) % MAX_MESSAGE_COUNT;

    mutex_unlock(&q->mutex_lock); 

    sem_signal(&q->sem_space); 

    return data;
}