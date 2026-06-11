#include <stdint.h>

#include "kernel.h"
#include "event_group.h"
#include "heap.h"
#include "ipc.h"
#include "scheduler.h"
#include "sync.h"
#include "task.h"
#include "uart.h"
#include "rtos_tests.h"

#ifdef MYOS_TEST_SCENARIO

#define RTOS_TEST_DELAY_TIMEOUT     1
#define RTOS_TEST_SEM_TIMEOUT       2
#define RTOS_TEST_SUSPEND_DELAY     3
#define RTOS_TEST_KILL_WAIT         4
#define RTOS_TEST_ROUND_ROBIN       5
#define RTOS_TEST_MUTEX_PI          6
#define RTOS_TEST_HEAP_FRAGMENT     7
#define RTOS_TEST_STACK_OVERFLOW    8
#define RTOS_TEST_MUTEX_OWNER       9
#define RTOS_TEST_QUEUE_TIMEOUT     10
#define RTOS_TEST_KILL_SUSP_MUTEX   11
#define RTOS_TEST_ISR_SEMAPHORE     12
#define RTOS_TEST_BINARY_SEMAPHORE  13
#define RTOS_TEST_COUNTING_SEMAPHORE 14
#define RTOS_TEST_RECURSIVE_MUTEX   15
#define RTOS_TEST_EVENT_GROUP       16
#define RTOS_TEST_SOFTWARE_TIMER    17

#define TEST_EVT_SENSOR_READY       (1U << 0)
#define TEST_EVT_CONTROL_READY      (1U << 1)

#define MAYBE_UNUSED __attribute__((unused))

static os_sem_t test_sem MAYBE_UNUSED;
static os_mutex_t test_mutex MAYBE_UNUSED;
static os_mutex_t test_mutex_b MAYBE_UNUSED;
static os_event_group_t test_events MAYBE_UNUSED;
static os_msg_queue_t test_queue MAYBE_UNUSED;
static os_timer_t test_oneshot_timer MAYBE_UNUSED;
static os_timer_t test_periodic_timer MAYBE_UNUSED;
static void *heap_blocks[384] MAYBE_UNUSED;

static volatile uint8_t test_done MAYBE_UNUSED;
static volatile uint8_t a_woke MAYBE_UNUSED;
static volatile uint32_t a_tid MAYBE_UNUSED;
static volatile uint32_t rr_last MAYBE_UNUSED;
static volatile uint32_t rr_switches MAYBE_UNUSED;
static volatile uint32_t rr_a_count MAYBE_UNUSED;
static volatile uint32_t rr_b_count MAYBE_UNUSED;
static volatile uint32_t low_unlocked MAYBE_UNUSED;
static volatile uint32_t high_acquired MAYBE_UNUSED;
static volatile uint32_t high_waiting MAYBE_UNUSED;
static volatile uint32_t medium_count MAYBE_UNUSED;
static volatile uint32_t owner_tid MAYBE_UNUSED;
static volatile uint32_t owner_locked MAYBE_UNUSED;
static volatile uint32_t owner_resume_seen MAYBE_UNUSED;
static volatile uint32_t owner_unlocked MAYBE_UNUSED;
static volatile os_status_t observed_status MAYBE_UNUSED;
static volatile uint32_t observed_bits MAYBE_UNUSED;
static volatile uint32_t timer_oneshot_count MAYBE_UNUSED;
static volatile uint32_t timer_periodic_count MAYBE_UNUSED;

static void MAYBE_UNUSED print_u32(const char *label, uint32_t value)
{
    uart_print(label);
    uart_print_dec(value);
}

static void MAYBE_UNUSED log_tick(const char *tag)
{
    uart_print("[RTOS_TEST] ");
    uart_print(tag);
    uart_print(" tick=");
    uart_print_dec(os_tick_count);
    uart_print("\r\n");
}

static void MAYBE_UNUSED finish_test(int pass, const char *name)
{
    if (test_done) {
        return;
    }

    test_done = 1;
    uart_print("[RTOS_TEST] ");
    uart_print(name);
    uart_print(pass ? " PASS\r\n" : " FAIL\r\n");
    uart_print("[RTOS_TEST_DONE]\r\n");
}

static void MAYBE_UNUSED idle_forever(void)
{
    while (1) {
        task_delay(1000);
    }
}

static uint32_t MAYBE_UNUSED find_tid_by_entry(void (*entry)(void))
{
    for (uint32_t tid = 0; tid < MAX_TASKS; tid++) {
        if (tcb_table[tid].entry == entry &&
            tcb_table[tid].state != TASK_UNUSED &&
            tcb_table[tid].state != TASK_TERMINATED) {
            return tid;
        }
    }

    return UINT32_MAX;
}

#if MYOS_TEST_SCENARIO == RTOS_TEST_DELAY_TIMEOUT
static void delay_timeout_task_a(void)
{
    uint32_t start_tick;
    uint32_t wake_tick;
    uint32_t delta;

    log_tick("A start");
    start_tick = os_tick_count;
    task_delay(10);
    wake_tick = os_tick_count;
    delta = wake_tick - start_tick;
    log_tick("A wake");
    print_u32("[RTOS_TEST] delay_delta=", delta);
    uart_print("\r\n");
    finish_test(delta >= 10U && delta <= 12U, "delay_timeout");
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_SEM_TIMEOUT
static void sem_timeout_task_a(void)
{
    uint32_t start_tick = os_tick_count;
    os_status_t result = sem_wait_timeout(&test_sem, 10);
    uint32_t delta = os_tick_count - start_tick;

    print_u32("[RTOS_TEST] sem_wait_result=", (uint32_t)result);
    print_u32(" delta=", delta);
    uart_print("\r\n");
    finish_test(result == OS_TIMEOUT && delta >= 10U && delta <= 12U,
                "semaphore_timeout");
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_SUSPEND_DELAY
static void suspend_delay_task_a(void)
{
    log_tick("A delay 100");
    task_delay(100);
    a_woke = 1;
    log_tick("A wake after resume");
    idle_forever();
}

static void suspend_delay_task_b(void)
{
    task_delay(1);
    a_tid = find_tid_by_entry(suspend_delay_task_a);
    task_delay(10);
    task_suspend(a_tid);
    log_tick("B suspended A");
    task_delay(120);

    if (a_woke != 0U) {
        finish_test(0, "suspend_delay");
        idle_forever();
    }

    task_resume(a_tid);
    log_tick("B resumed A");
    task_delay(5);
    finish_test(a_woke != 0U, "suspend_delay");
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_KILL_WAIT
static void kill_wait_task_a(void)
{
    log_tick("A wait semaphore forever");
    sem_wait(&test_sem);
    a_woke = 1;
    log_tick("A woke unexpectedly");
    idle_forever();
}

static void kill_wait_task_b(void)
{
    task_delay(1);
    a_tid = find_tid_by_entry(kill_wait_task_a);
    task_delay(10);
    task_kill(a_tid);
    log_tick("B killed A");
    sem_signal(&test_sem);
    task_delay(5);
    finish_test(a_woke == 0U, "kill_wait");
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_ROUND_ROBIN
static void rr_note(uint32_t id)
{
    if (rr_last != 0U && rr_last != id) {
        rr_switches++;
    }

    rr_last = id;
}

static void round_robin_task_a(void)
{
    for (uint32_t i = 0; i < 8; i++) {
        rr_a_count++;
        rr_note(1);
        log_tick("A rr");
        task_delay(1);
    }

    task_exit();
}

static void round_robin_task_b(void)
{
    for (uint32_t i = 0; i < 8; i++) {
        rr_b_count++;
        rr_note(2);
        log_tick("B rr");
        task_delay(1);
    }

    task_exit();
}

static void round_robin_check_task(void)
{
    task_delay(40);
    print_u32("[RTOS_TEST] rr_a=", rr_a_count);
    print_u32(" rr_b=", rr_b_count);
    print_u32(" switches=", rr_switches);
    uart_print("\r\n");
    finish_test(rr_a_count == 8U && rr_b_count == 8U && rr_switches >= 10U,
                "round_robin");
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_MUTEX_PI
static void mutex_pi_low_task(void)
{
    mutex_lock(&test_mutex);
    log_tick("Low lock");

    while (high_waiting == 0U) {
        task_yield();
    }

    for (uint32_t i = 0; i < 200U; i++) {
        task_yield();
    }

    low_unlocked = os_tick_count;
    log_tick("Low unlock");
    mutex_unlock(&test_mutex);
    idle_forever();
}

static void mutex_pi_high_task(void)
{
    task_delay(5);
    log_tick("High wait mutex");
    high_waiting = 1;
    mutex_lock(&test_mutex);
    high_acquired = os_tick_count;
    log_tick("High acquired mutex");
    mutex_unlock(&test_mutex);
    idle_forever();
}

static void mutex_pi_medium_task(void)
{
    task_delay(10);

    while (high_acquired == 0U) {
        medium_count++;
        task_yield();
    }

    log_tick("Medium observed High done");
    idle_forever();
}

static void mutex_pi_check_task(void)
{
    task_delay(80);
    print_u32("[RTOS_TEST] low_unlock=", low_unlocked);
    print_u32(" high_acquired=", high_acquired);
    print_u32(" medium_count=", medium_count);
    uart_print("\r\n");
    finish_test(low_unlocked != 0U &&
                high_acquired >= low_unlocked &&
                (high_acquired - low_unlocked) <= 2U &&
                medium_count == 0U,
                "mutex_priority_inheritance");
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_HEAP_FRAGMENT
static void heap_fragment_task(void)
{
    void *fragment_probe;
    void *coalesced_probe;
    uint32_t allocated = 0;

    for (uint32_t i = 0; i < 384U; i++) {
        heap_blocks[i] = os_malloc(512);
        if (heap_blocks[i] == NULL) {
            break;
        }
        allocated++;
    }

    for (uint32_t i = 1; i + 1U < allocated; i += 2U) {
        os_free(heap_blocks[i]);
        heap_blocks[i] = NULL;
    }

    fragment_probe = os_malloc(768);
    if (fragment_probe != NULL) {
        os_free(fragment_probe);
    }

    for (uint32_t i = 0; i < allocated; i++) {
        if (heap_blocks[i] != NULL) {
            os_free(heap_blocks[i]);
            heap_blocks[i] = NULL;
        }
    }

    coalesced_probe = os_malloc(1024);
    if (coalesced_probe != NULL) {
        os_free(coalesced_probe);
    }

    print_u32("[RTOS_TEST] heap_blocks=", allocated);
    uart_print("\r\n");
    finish_test(allocated >= 4U &&
                fragment_probe == NULL &&
                coalesced_probe != NULL,
                "heap_fragmentation");
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_STACK_OVERFLOW
static void stack_overflow_task(void)
{
    log_tick("corrupt stack canary");
    *((uint32_t *)current_tcb->mem.stack_base) = 0U;
    task_check_stack(current_tcb);
    finish_test(0, "stack_overflow");
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_MUTEX_OWNER
static void mutex_owner_a(void)
{
    os_status_t first = mutex_lock_timeout(&test_mutex, 0);
    os_status_t second = mutex_lock_timeout(&test_mutex, 0);

    print_u32("[RTOS_TEST] recursive_first=", (uint32_t)first);
    print_u32(" second=", (uint32_t)second);
    print_u32(" lock_count=", test_mutex.lock_count);
    uart_print("\r\n");

    owner_locked = (first == OS_OK &&
                    second == OS_OK &&
                    test_mutex.owner == current_tcb &&
                    test_mutex.lock_count == 2U);
    task_delay(20);
    mutex_unlock(&test_mutex);
    mutex_unlock(&test_mutex);
    idle_forever();
}

static void mutex_owner_b(void)
{
    task_delay(5);
    mutex_unlock(&test_mutex);
    observed_status = mutex_lock_timeout(&test_mutex, 0);

    print_u32("[RTOS_TEST] wrong_owner_lock_result=", (uint32_t)observed_status);
    print_u32(" lock_count=", test_mutex.lock_count);
    uart_print("\r\n");

    finish_test(owner_locked != 0U &&
                observed_status == OS_TIMEOUT &&
                test_mutex.owner != current_tcb &&
                test_mutex.lock_count == 2U,
                "mutex_recursive_owner");
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_QUEUE_TIMEOUT
static void queue_timeout_task(void)
{
    os_status_t send_status = OS_OK;
    os_status_t recv_status;
    int32_t data = 0;
    uint32_t send_start;
    uint32_t recv_start;
    uint32_t send_delta;
    uint32_t recv_delta;

    for (uint32_t i = 0; i < MAX_MESSAGE_COUNT; i++) {
        data = (int32_t)i;
        if (msg_queue_send_timeout(&test_queue, &data, 0) != OS_OK) {
            finish_test(0, "queue_timeout");
            idle_forever();
        }
    }

    send_start = os_tick_count;
    data = 1234;
    send_status = msg_queue_send_timeout(&test_queue, &data, 5);
    send_delta = os_tick_count - send_start;

    while (msg_queue_receive_timeout(&test_queue, &data, 0) == OS_OK) {
    }

    recv_start = os_tick_count;
    recv_status = msg_queue_receive_timeout(&test_queue, &data, 5);
    recv_delta = os_tick_count - recv_start;

    print_u32("[RTOS_TEST] q_send=", (uint32_t)send_status);
    print_u32(" send_delta=", send_delta);
    print_u32(" q_recv=", (uint32_t)recv_status);
    print_u32(" recv_delta=", recv_delta);
    uart_print("\r\n");

    finish_test(send_status == OS_TIMEOUT &&
                recv_status == OS_TIMEOUT &&
                send_delta >= 5U &&
                recv_delta >= 5U,
                "queue_timeout");
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_KILL_SUSP_MUTEX
static void kill_mutex_owner_task(void)
{
    mutex_lock(&test_mutex);
    owner_locked = 1;
    while (1) {
        task_delay(1000);
    }
}

static void suspend_mutex_owner_task(void)
{
    mutex_lock(&test_mutex);
    owner_locked = 1;

    while (owner_resume_seen == 0U) {
        task_delay(1);
    }

    mutex_unlock(&test_mutex);
    owner_unlocked = 1;
    idle_forever();
}

static void kill_suspend_mutex_check_task(void)
{
    os_status_t kill_lock_status;
    os_status_t suspend_wait_status;
    os_status_t resume_lock_status;

    task_delay(2);
    owner_tid = find_tid_by_entry(kill_mutex_owner_task);
    if (owner_locked == 0U || owner_tid == UINT32_MAX) {
        finish_test(0, "kill_suspend_mutex");
        idle_forever();
    }

    task_kill(owner_tid);
    task_delay(1);
    kill_lock_status = mutex_lock_timeout(&test_mutex, 2);
    if (kill_lock_status == OS_OK) {
        mutex_unlock(&test_mutex);
    }

    mutex_init(&test_mutex);
    owner_locked = 0;
    owner_resume_seen = 0;
    owner_unlocked = 0;

    task_create(suspend_mutex_owner_task, PRIORITY_NORMAL);
    task_delay(2);
    owner_tid = find_tid_by_entry(suspend_mutex_owner_task);
    if (owner_locked == 0U || owner_tid == UINT32_MAX) {
        finish_test(0, "kill_suspend_mutex");
        idle_forever();
    }

    task_suspend(owner_tid);
    suspend_wait_status = mutex_lock_timeout(&test_mutex, 3);
    owner_resume_seen = 1;
    task_resume(owner_tid);
    task_delay(3);
    resume_lock_status = mutex_lock_timeout(&test_mutex, 3);
    if (resume_lock_status == OS_OK) {
        mutex_unlock(&test_mutex);
    }

    print_u32("[RTOS_TEST] kill_lock=", (uint32_t)kill_lock_status);
    print_u32(" suspend_wait=", (uint32_t)suspend_wait_status);
    print_u32(" resume_lock=", (uint32_t)resume_lock_status);
    uart_print("\r\n");

    finish_test(kill_lock_status == OS_OK &&
                suspend_wait_status == OS_TIMEOUT &&
                owner_unlocked != 0U &&
                resume_lock_status == OS_OK,
                "kill_suspend_mutex");
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_ISR_SEMAPHORE
static void isr_sem_waiter_task(void)
{
    os_status_t status = sem_wait_timeout(&test_sem, 20);
    print_u32("[RTOS_TEST] isr_sem_wait=", (uint32_t)status);
    uart_print("\r\n");
    finish_test(status == OS_OK, "isr_semaphore");
    idle_forever();
}

static void isr_sem_signal_task(void)
{
    task_delay(3);
    sem_signal_from_isr(&test_sem);
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_BINARY_SEMAPHORE
static void binary_sem_cap_task(void)
{
    os_status_t first;
    os_status_t second;

    sem_signal(&test_sem);
    sem_signal(&test_sem);

    first = sem_wait_timeout(&test_sem, 0);
    second = sem_wait_timeout(&test_sem, 2);

    print_u32("[RTOS_TEST] binary_first=", (uint32_t)first);
    print_u32(" binary_second=", (uint32_t)second);
    uart_print("\r\n");

    finish_test(first == OS_OK && second == OS_TIMEOUT,
                "binary_semaphore");
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_COUNTING_SEMAPHORE
static void counting_sem_task(void)
{
    os_status_t init_status;
    os_status_t event_a;
    os_status_t event_b;
    os_status_t event_c;
    os_status_t event_d;
    os_status_t res_a;
    os_status_t res_b;
    os_status_t res_c;
    int32_t event_count;
    int32_t resource_count;

    init_status = sem_init_counting(&test_sem, 0, 3);

    sem_signal(&test_sem);
    sem_signal(&test_sem);
    sem_signal(&test_sem);
    sem_signal(&test_sem);
    event_count = sem_get_count(&test_sem);

    event_a = sem_wait_timeout(&test_sem, 0);
    event_b = sem_wait_timeout(&test_sem, 0);
    event_c = sem_wait_timeout(&test_sem, 0);
    event_d = sem_wait_timeout(&test_sem, 1);

    init_status = (init_status == OS_OK)
        ? sem_init_counting(&test_sem, 2, 2)
        : init_status;
    res_a = sem_wait_timeout(&test_sem, 0);
    res_b = sem_wait_timeout(&test_sem, 0);
    res_c = sem_wait_timeout(&test_sem, 1);
    sem_signal(&test_sem);
    resource_count = sem_get_count(&test_sem);

    print_u32("[RTOS_TEST] counting_event_count=", (uint32_t)event_count);
    print_u32(" resource_count=", (uint32_t)resource_count);
    uart_print("\r\n");

    finish_test(init_status == OS_OK &&
                event_count == 3 &&
                event_a == OS_OK &&
                event_b == OS_OK &&
                event_c == OS_OK &&
                event_d == OS_TIMEOUT &&
                res_a == OS_OK &&
                res_b == OS_OK &&
                res_c == OS_TIMEOUT &&
                resource_count == 1,
                "counting_semaphore");
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_RECURSIVE_MUTEX
static void recursive_mutex_task(void)
{
    os_status_t first;
    os_status_t second;
    os_status_t third;
    uint32_t held_after_locks;
    uint32_t held_after_one_unlock;
    uint32_t held_after_b_unlock;
    uint32_t held_after_all_unlock;

    first = recursive_mutex_lock_timeout(&test_mutex, 0);
    second = recursive_mutex_lock_timeout(&test_mutex, 0);
    third = recursive_mutex_lock_timeout(&test_mutex_b, 0);
    held_after_locks = current_tcb->mutexes_held_count;

    recursive_mutex_unlock(&test_mutex);
    held_after_one_unlock = current_tcb->mutexes_held_count;

    recursive_mutex_unlock(&test_mutex_b);
    held_after_b_unlock = current_tcb->mutexes_held_count;

    recursive_mutex_unlock(&test_mutex);
    held_after_all_unlock = current_tcb->mutexes_held_count;

    print_u32("[RTOS_TEST] recursive_held_locks=", held_after_locks);
    print_u32(" after_one=", held_after_one_unlock);
    print_u32(" after_b=", held_after_b_unlock);
    print_u32(" after_all=", held_after_all_unlock);
    uart_print("\r\n");

    finish_test(first == OS_OK &&
                second == OS_OK &&
                third == OS_OK &&
                test_mutex.lock_count == 0U &&
                test_mutex_b.lock_count == 0U &&
                held_after_locks == 2U &&
                held_after_one_unlock == 2U &&
                held_after_b_unlock == 1U &&
                held_after_all_unlock == 0U,
                "recursive_mutex");
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_EVENT_GROUP
static void event_group_waiter_task(void)
{
    observed_bits = event_group_wait_bits(&test_events,
                                          TEST_EVT_SENSOR_READY |
                                              TEST_EVT_CONTROL_READY,
                                          1,
                                          1,
                                          20);

    print_u32("[RTOS_TEST] event_bits=", observed_bits);
    print_u32(" remaining=", event_group_get_bits(&test_events));
    uart_print("\r\n");

    finish_test((observed_bits & (TEST_EVT_SENSOR_READY |
                                  TEST_EVT_CONTROL_READY)) ==
                    (TEST_EVT_SENSOR_READY | TEST_EVT_CONTROL_READY) &&
                    event_group_get_bits(&test_events) == 0U,
                "event_group");
    idle_forever();
}

static void event_group_sensor_task(void)
{
    task_delay(2);
    event_group_set_bits(&test_events, TEST_EVT_SENSOR_READY);
    idle_forever();
}

static void event_group_control_task(void)
{
    task_delay(4);
    event_group_set_bits(&test_events, TEST_EVT_CONTROL_READY);
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_SOFTWARE_TIMER
static void software_timer_oneshot_cb(void *arg)
{
    (void)arg;
    timer_oneshot_count++;
}

static void software_timer_periodic_cb(void *arg)
{
    os_timer_t *timer = (os_timer_t *)arg;

    timer_periodic_count++;
    if (timer_periodic_count >= 3U) {
        os_timer_stop(timer);
    }
}

static void software_timer_check_task(void)
{
    os_timer_init(&test_oneshot_timer, software_timer_oneshot_cb, NULL);
    os_timer_init(&test_periodic_timer,
                  software_timer_periodic_cb,
                  &test_periodic_timer);

    if (os_timer_start(&test_oneshot_timer,
                       3U,
                       OS_TIMER_ONESHOT) != OS_OK ||
        os_timer_start(&test_periodic_timer,
                       2U,
                       OS_TIMER_PERIODIC) != OS_OK) {
        finish_test(0, "software_timer");
        idle_forever();
    }

    task_delay(10);

    print_u32("[RTOS_TEST] oneshot_count=", timer_oneshot_count);
    print_u32(" periodic_count=", timer_periodic_count);
    uart_print("\r\n");

    finish_test(timer_oneshot_count == 1U &&
                    timer_periodic_count == 3U &&
                    os_timer_is_active(&test_oneshot_timer) == 0U &&
                    os_timer_is_active(&test_periodic_timer) == 0U,
                "software_timer");
    idle_forever();
}
#endif

void rtos_test_init(void)
{
    test_done = 0;
    a_woke = 0;
    a_tid = UINT32_MAX;
    rr_last = 0;
    rr_switches = 0;
    rr_a_count = 0;
    rr_b_count = 0;
    low_unlocked = 0;
    high_acquired = 0;
    high_waiting = 0;
    medium_count = 0;
    owner_tid = UINT32_MAX;
    owner_locked = 0;
    owner_resume_seen = 0;
    owner_unlocked = 0;
    observed_status = OS_ERROR;
    observed_bits = 0U;
    timer_oneshot_count = 0U;
    timer_periodic_count = 0U;

    binary_sem_init(&test_sem, 0);
    mutex_init(&test_mutex);
    mutex_init(&test_mutex_b);
    event_group_init(&test_events);
    if (msg_queue_init(&test_queue, MAX_MESSAGE_COUNT, sizeof(int32_t)) != OS_OK) {
        finish_test(0, "queue_init");
        return;
    }

    uart_print("[RTOS_TEST] scenario=");
    uart_print_dec(MYOS_TEST_SCENARIO);
    uart_print("\r\n");

#if MYOS_TEST_SCENARIO == RTOS_TEST_DELAY_TIMEOUT
    task_create(delay_timeout_task_a, PRIORITY_NORMAL);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_SEM_TIMEOUT
    task_create(sem_timeout_task_a, PRIORITY_NORMAL);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_SUSPEND_DELAY
    task_create(suspend_delay_task_a, PRIORITY_NORMAL);
    task_create(suspend_delay_task_b, PRIORITY_HIGH);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_KILL_WAIT
    task_create(kill_wait_task_a, PRIORITY_NORMAL);
    task_create(kill_wait_task_b, PRIORITY_HIGH);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_ROUND_ROBIN
    task_create(round_robin_task_a, PRIORITY_NORMAL);
    task_create(round_robin_task_b, PRIORITY_NORMAL);
    task_create(round_robin_check_task, PRIORITY_LOW);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_MUTEX_PI
    task_create(mutex_pi_low_task, PRIORITY_LOW);
    task_create(mutex_pi_high_task, PRIORITY_HIGH);
    task_create(mutex_pi_medium_task, PRIORITY_NORMAL);
    task_create(mutex_pi_check_task, PRIORITY_LOW);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_HEAP_FRAGMENT
    task_create(heap_fragment_task, PRIORITY_NORMAL);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_STACK_OVERFLOW
    task_create_dynamic(stack_overflow_task, PRIORITY_NORMAL,
                        OS_MIN_STACK_WORDS, NULL);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_MUTEX_OWNER
    task_create(mutex_owner_a, PRIORITY_NORMAL);
    task_create(mutex_owner_b, PRIORITY_HIGH);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_QUEUE_TIMEOUT
    task_create(queue_timeout_task, PRIORITY_NORMAL);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_KILL_SUSP_MUTEX
    task_create(kill_mutex_owner_task, PRIORITY_NORMAL);
    task_create(kill_suspend_mutex_check_task, PRIORITY_HIGH);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_ISR_SEMAPHORE
    task_create(isr_sem_waiter_task, PRIORITY_NORMAL);
    task_create(isr_sem_signal_task, PRIORITY_HIGH);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_BINARY_SEMAPHORE
    task_create(binary_sem_cap_task, PRIORITY_NORMAL);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_COUNTING_SEMAPHORE
    task_create(counting_sem_task, PRIORITY_NORMAL);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_RECURSIVE_MUTEX
    task_create(recursive_mutex_task, PRIORITY_NORMAL);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_EVENT_GROUP
    task_create(event_group_waiter_task, PRIORITY_HIGH);
    task_create(event_group_sensor_task, PRIORITY_NORMAL);
    task_create(event_group_control_task, PRIORITY_NORMAL);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_SOFTWARE_TIMER
    task_create(software_timer_check_task, PRIORITY_NORMAL);
#else
    uart_print("[RTOS_TEST] unknown scenario\r\n");
    uart_print("[RTOS_TEST_DONE]\r\n");
#endif
}

#else

void rtos_test_init(void)
{
}

#endif
