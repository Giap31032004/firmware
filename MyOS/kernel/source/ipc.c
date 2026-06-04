#include "ipc.h"
#include "critical.h"
#include "heap.h"
#include "os_trace.h"
#include "scheduler.h"
#include "task.h"

typedef enum {
    MSG_QUEUE_SEND_BACK = 0,
    MSG_QUEUE_SEND_FRONT
} msg_queue_send_pos_t;

static void msg_queue_copy(uint8_t *dst, const uint8_t *src, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++) {
        dst[i] = src[i];
    }
}

static int msg_queue_is_ready(const os_msg_queue_t *q)
{
    return q != NULL && q->buffer != NULL && q->length != 0U && q->item_size != 0U;
}

static int msg_queue_accepts_buffer(const os_msg_queue_t *q)
{
    return msg_queue_is_ready(q) && q->item_size == sizeof(os_msg_buffer_t);
}

static void msg_buffer_clear(os_msg_buffer_t *msg)
{
    msg->data = NULL;
    msg->size = 0U;
}

static void msg_queue_push_item(os_msg_queue_t *q,
                                const void *item,
                                msg_queue_send_pos_t pos)
{
    uint8_t *slot;

    if (pos == MSG_QUEUE_SEND_FRONT) {
        q->tail = (q->tail + (int)q->length - 1) % (int)q->length;
        slot = q->buffer + ((uint32_t)q->tail * q->item_size);
    } else {
        slot = q->buffer + ((uint32_t)q->head * q->item_size);
        q->head = (q->head + 1) % (int)q->length;
    }

    msg_queue_copy(slot, (const uint8_t *)item, q->item_size);
}

os_status_t msg_queue_init(os_msg_queue_t *q, uint32_t length, uint32_t item_size)
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
    if (sem_init_counting(&q->sem_data, 0, (int32_t)length) != OS_OK ||
        sem_init_counting(&q->sem_space, (int32_t)length, (int32_t)length) != OS_OK) {
        os_free(q->buffer);
        q->buffer = NULL;
        return OS_ERROR;
    }

    return OS_OK;
}

static os_status_t msg_queue_send_at_timeout(os_msg_queue_t *q,
                                             const void *item,
                                             uint32_t timeout_ticks,
                                             msg_queue_send_pos_t pos)
{
    os_status_t status;

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

    {
        uint32_t irq_state = os_enter_critical();

        msg_queue_push_item(q, item, pos);
        MYOS_TRACE(OS_TRACE_QUEUE_SEND,
                   current_tcb != NULL ? current_tcb->tid : UINT32_MAX,
                   (uint32_t)q,
                   (uint32_t)q->head);

        os_exit_critical(irq_state);
    }

    mutex_unlock(&q->mutex_lock);
    sem_signal(&q->sem_data);

    return OS_OK;
}

os_status_t msg_queue_send_timeout(os_msg_queue_t *q, const void *item, uint32_t timeout_ticks)
{
    return msg_queue_send_to_back_timeout(q, item, timeout_ticks);
}

os_status_t msg_queue_send_to_back_timeout(os_msg_queue_t *q,
                                           const void *item,
                                           uint32_t timeout_ticks)
{
    return msg_queue_send_at_timeout(q, item, timeout_ticks, MSG_QUEUE_SEND_BACK);
}

os_status_t msg_queue_send_to_front_timeout(os_msg_queue_t *q,
                                            const void *item,
                                            uint32_t timeout_ticks)
{
    return msg_queue_send_at_timeout(q, item, timeout_ticks, MSG_QUEUE_SEND_FRONT);
}

void msg_queue_send(os_msg_queue_t *q, const void *item)
{
    msg_queue_send_to_back(q, item);
}

void msg_queue_send_to_back(os_msg_queue_t *q, const void *item)
{
    (void)msg_queue_send_to_back_timeout(q, item, OS_WAIT_FOREVER);
}

void msg_queue_send_to_front(os_msg_queue_t *q, const void *item)
{
    (void)msg_queue_send_to_front_timeout(q, item, OS_WAIT_FOREVER);
}

os_status_t msg_queue_receive_timeout(os_msg_queue_t *q, void *item, uint32_t timeout_ticks)
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

    {
        uint32_t irq_state = os_enter_critical();

        slot = q->buffer + ((uint32_t)q->tail * q->item_size);
        msg_queue_copy((uint8_t *)item, slot, q->item_size);
        q->tail = (q->tail + 1) % (int)q->length;
        MYOS_TRACE(OS_TRACE_QUEUE_RECEIVE,
                   current_tcb != NULL ? current_tcb->tid : UINT32_MAX,
                   (uint32_t)q,
                   (uint32_t)q->tail);

        os_exit_critical(irq_state);
    }

    mutex_unlock(&q->mutex_lock);
    sem_signal(&q->sem_space);

    return OS_OK;
}

void msg_queue_receive(os_msg_queue_t *q, void *item)
{
    (void)msg_queue_receive_timeout(q, item, OS_WAIT_FOREVER);
}

static os_status_t msg_queue_send_at_from_isr(os_msg_queue_t *q,
                                              const void *item,
                                              msg_queue_send_pos_t pos)
{
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

    msg_queue_push_item(q, item, pos);
    MYOS_TRACE(OS_TRACE_QUEUE_SEND, UINT32_MAX, (uint32_t)q, (uint32_t)q->head);

    os_exit_critical(irq_state);

    sem_signal_from_isr(&q->sem_data);

    return OS_OK;
}

os_status_t msg_queue_send_from_isr(os_msg_queue_t *q, const void *item)
{
    return msg_queue_send_to_back_from_isr(q, item);
}

os_status_t msg_queue_send_to_back_from_isr(os_msg_queue_t *q, const void *item)
{
    return msg_queue_send_at_from_isr(q, item, MSG_QUEUE_SEND_BACK);
}

os_status_t msg_queue_send_to_front_from_isr(os_msg_queue_t *q, const void *item)
{
    return msg_queue_send_at_from_isr(q, item, MSG_QUEUE_SEND_FRONT);
}

os_status_t msg_queue_receive_from_isr(os_msg_queue_t *q, void *item)
{
    uint32_t irq_state;
    uint8_t *slot;

    if (!msg_queue_is_ready(q) || item == NULL) {
        return OS_ERROR;
    }

    irq_state = os_enter_critical();

    if (q->sem_data.count <= 0) {
        os_exit_critical(irq_state);
        return OS_TIMEOUT;
    }

    q->sem_data.count--;

    slot = q->buffer + ((uint32_t)q->tail * q->item_size);
    msg_queue_copy((uint8_t *)item, slot, q->item_size);
    q->tail = (q->tail + 1) % (int)q->length;
    MYOS_TRACE(OS_TRACE_QUEUE_RECEIVE, UINT32_MAX, (uint32_t)q, (uint32_t)q->tail);

    os_exit_critical(irq_state);

    sem_signal_from_isr(&q->sem_space);

    return OS_OK;
}

os_status_t msg_buffer_alloc(os_msg_buffer_t *msg, uint32_t size)
{
    if (msg == NULL || size == 0U) {
        return OS_ERROR;
    }

    msg->data = os_malloc(size);
    if (msg->data == NULL) {
        msg->size = 0U;
        return OS_ERROR;
    }

    msg->size = size;
    return OS_OK;
}

void msg_buffer_release(os_msg_buffer_t *msg)
{
    if (msg == NULL) {
        return;
    }

    if (msg->data != NULL) {
        os_free(msg->data);
    }

    msg_buffer_clear(msg);
}

os_status_t msg_queue_send_buffer_timeout(os_msg_queue_t *q,
                                          os_msg_buffer_t *msg,
                                          uint32_t timeout_ticks)
{
    os_status_t status;

    if (!msg_queue_accepts_buffer(q) ||
        msg == NULL ||
        msg->data == NULL ||
        msg->size == 0U) {
        return OS_ERROR;
    }

    status = msg_queue_send_to_back_timeout(q, msg, timeout_ticks);
    if (status == OS_OK) {
        msg_buffer_clear(msg);
    }

    return status;
}

os_status_t msg_queue_receive_buffer_timeout(os_msg_queue_t *q,
                                             os_msg_buffer_t *msg,
                                             uint32_t timeout_ticks)
{
    if (!msg_queue_accepts_buffer(q) || msg == NULL) {
        return OS_ERROR;
    }

    msg_buffer_clear(msg);
    return msg_queue_receive_timeout(q, msg, timeout_ticks);
}

os_status_t msg_queue_send_buffer_from_isr(os_msg_queue_t *q,
                                           os_msg_buffer_t *msg)
{
    os_status_t status;

    if (!msg_queue_accepts_buffer(q) ||
        msg == NULL ||
        msg->data == NULL ||
        msg->size == 0U) {
        return OS_ERROR;
    }

    status = msg_queue_send_to_back_from_isr(q, msg);
    if (status == OS_OK) {
        msg_buffer_clear(msg);
    }

    return status;
}

os_status_t msg_queue_receive_buffer_from_isr(os_msg_queue_t *q,
                                              os_msg_buffer_t *msg)
{
    if (!msg_queue_accepts_buffer(q) || msg == NULL) {
        return OS_ERROR;
    }

    msg_buffer_clear(msg);
    return msg_queue_receive_from_isr(q, msg);
}
