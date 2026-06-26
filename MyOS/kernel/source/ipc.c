#include "ipc.h"

#include "critical.h"
#include "heap.h"
#include "os_trace.h"
#include "task.h"

static void msg_queue_copy(uint8_t *dst, const uint8_t *src, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++) {
        dst[i] = src[i];
    }
}

static int msg_queue_is_ready(const os_msg_queue_t *q)
{
    return q != NULL &&
           q->buffer != NULL &&
           q->length != 0U &&
           q->item_size != 0U;
}

os_status_t msg_queue_init(os_msg_queue_t *q,
                           uint32_t length,
                           uint32_t item_size)
{
    uint32_t buffer_size;

    if (q == NULL ||
        length == 0U ||
        item_size == 0U ||
        length > 0x7fffffffU ||
        item_size > (UINT32_MAX / length)) {
        return OS_ERROR;
    }

    buffer_size = length * item_size;
    q->buffer = os_malloc(buffer_size);
    if (q->buffer == NULL) {
        return OS_ERROR;
    }

    q->length = length;
    q->item_size = item_size;
    q->head = 0;
    q->tail = 0;

    mutex_init(&q->mutex_lock);
    if (sem_init(&q->sem_data, 0, (int32_t)length) != OS_OK ||
        sem_init(&q->sem_space,
                 (int32_t)length,
                 (int32_t)length) != OS_OK) {
        os_free(q->buffer);
        q->buffer = NULL;
        return OS_ERROR;
    }

    return OS_OK;
}

os_status_t msg_queue_send_timeout(os_msg_queue_t *q,
                                   const void *item,
                                   uint32_t timeout_ticks)
{
    os_status_t status;
    uint8_t *slot;

    if (!msg_queue_is_ready(q) || item == NULL) {
        return OS_ERROR;
    }

    status = sem_wait_timeout(&q->sem_space, timeout_ticks);
    if (status != OS_OK) {
        return status;
    }

    status = mutex_lock_timeout(&q->mutex_lock, timeout_ticks);
    if (status != OS_OK) {
        sem_signal(&q->sem_space);
        return status;
    }

    uint32_t irq_state = os_enter_critical();
    slot = q->buffer + ((uint32_t)q->head * q->item_size);
    msg_queue_copy(slot, (const uint8_t *)item, q->item_size);
    q->head = (q->head + 1) % (int)q->length;
    os_exit_critical(irq_state);

    MYOS_TRACE(OS_TRACE_QUEUE_SEND,
               current_tcb != NULL ? current_tcb->tid : UINT32_MAX,
               (uint32_t)q,
               (uint32_t)q->head);

    mutex_unlock(&q->mutex_lock);
    sem_signal(&q->sem_data);
    return OS_OK;
}

void msg_queue_send(os_msg_queue_t *q, const void *item)
{
    (void)msg_queue_send_timeout(q, item, OS_WAIT_FOREVER);
}

os_status_t msg_queue_receive_timeout(os_msg_queue_t *q,
                                      void *item,
                                      uint32_t timeout_ticks)
{
    os_status_t status;
    uint8_t *slot;

    if (!msg_queue_is_ready(q) || item == NULL) {
        return OS_ERROR;
    }

    status = sem_wait_timeout(&q->sem_data, timeout_ticks);
    if (status != OS_OK) {
        return status;
    }

    status = mutex_lock_timeout(&q->mutex_lock, timeout_ticks);
    if (status != OS_OK) {
        sem_signal(&q->sem_data);
        return status;
    }

    uint32_t irq_state = os_enter_critical();
    slot = q->buffer + ((uint32_t)q->tail * q->item_size);
    msg_queue_copy((uint8_t *)item, slot, q->item_size);
    q->tail = (q->tail + 1) % (int)q->length;
    os_exit_critical(irq_state);

    MYOS_TRACE(OS_TRACE_QUEUE_RECEIVE,
               current_tcb != NULL ? current_tcb->tid : UINT32_MAX,
               (uint32_t)q,
               (uint32_t)q->tail);

    mutex_unlock(&q->mutex_lock);
    sem_signal(&q->sem_space);
    return OS_OK;
}

void msg_queue_receive(os_msg_queue_t *q, void *item)
{
    (void)msg_queue_receive_timeout(q, item, OS_WAIT_FOREVER);
}

os_status_t msg_queue_send_from_isr(os_msg_queue_t *q, const void *item)
{
    uint8_t *slot;
    uint32_t irq_state;

    if (!msg_queue_is_ready(q) || item == NULL) {
        return OS_ERROR;
    }

    irq_state = os_enter_critical();

    if (q->sem_space.count <= 0) {
        os_exit_critical(irq_state);
        return OS_TIMEOUT;
    }

    q->sem_space.count--;
    slot = q->buffer + ((uint32_t)q->head * q->item_size);
    msg_queue_copy(slot, (const uint8_t *)item, q->item_size);
    q->head = (q->head + 1) % (int)q->length;

    MYOS_TRACE(OS_TRACE_QUEUE_SEND,
               UINT32_MAX,
               (uint32_t)q,
               (uint32_t)q->head);

    os_exit_critical(irq_state);

    sem_signal_from_isr(&q->sem_data);
    return OS_OK;
}
