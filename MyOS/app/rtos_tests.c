#include <stdint.h>

#include "kernel.h"
#include "hardware_config.h"
#include "heap.h"
#include "ipc.h"
#include "runtime_stats.h"
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
#define RTOS_TEST_QUEUE_TIMEOUT     10
#define RTOS_TEST_ISR_SEMAPHORE     12
#define RTOS_TEST_BINARY_SEMAPHORE  13
#define RTOS_TEST_COUNTING_SEMAPHORE 14
#define RTOS_TEST_QUEUE_ISR         15
#define RTOS_TEST_SOFTWARE_TIMER    17
#define RTOS_TEST_API_LATENCY       18
#define RTOS_TEST_CONTEXT_SWITCH    19
#define RTOS_TEST_TIMER_JITTER      20
#define RTOS_TEST_CPU_LOAD          21

#define RTOS_BENCH_ITERATIONS       64U
#define RTOS_BENCH_PERIOD_TICKS     10U
#define RTOS_BENCH_LOAD_TICKS       1000U

#ifndef MYOS_BENCH_USE_DWT
#define MYOS_BENCH_USE_DWT          0
#endif

#define MAYBE_UNUSED __attribute__((unused))

static os_sem_t test_sem MAYBE_UNUSED;
static os_mutex_t test_mutex MAYBE_UNUSED;
static os_msg_queue_t test_queue MAYBE_UNUSED;
static os_timer_t test_oneshot_timer MAYBE_UNUSED;
static os_timer_t test_periodic_timer MAYBE_UNUSED;
static void *heap_blocks[384] MAYBE_UNUSED;
static volatile uint32_t bench_sink MAYBE_UNUSED;

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
static volatile uint32_t timer_oneshot_count MAYBE_UNUSED;
static volatile uint32_t timer_periodic_count MAYBE_UNUSED;
static volatile uint32_t cs_phase MAYBE_UNUSED;
static volatile uint32_t cs_start_cycles MAYBE_UNUSED;
static volatile uint32_t cs_samples MAYBE_UNUSED;
static volatile uint32_t cs_min_cycles MAYBE_UNUSED;
static volatile uint32_t cs_max_cycles MAYBE_UNUSED;
static volatile uint32_t cs_sum_cycles MAYBE_UNUSED;
static volatile uint32_t cpu_load_busy_count MAYBE_UNUSED;

static void MAYBE_UNUSED print_u32(const char *label, uint32_t value)
{
    uart_print(label);
    uart_print_dec(value);
}

static void MAYBE_UNUSED print_fixed_x100(uint32_t value)
{
    uart_print_dec(value / 100U);
    uart_print(".");
    if ((value % 100U) < 10U) {
        uart_print("0");
    }
    uart_print_dec(value % 100U);
}

static void MAYBE_UNUSED bench_dwt_init(void)
{
#if MYOS_BENCH_USE_DWT
    volatile uint32_t *demcr = (volatile uint32_t *)0xE000EDFCUL;
    volatile uint32_t *dwt_cyccnt = (volatile uint32_t *)0xE0001004UL;
    volatile uint32_t *dwt_ctrl = (volatile uint32_t *)0xE0001000UL;

    *demcr |= (1UL << 24);
    *dwt_cyccnt = 0U;
    *dwt_ctrl |= 1UL;
#endif
}

static uint32_t MAYBE_UNUSED bench_cycles_now(void)
{
#if MYOS_BENCH_USE_DWT
    volatile uint32_t *dwt_cyccnt = (volatile uint32_t *)0xE0001004UL;

    return *dwt_cyccnt;
#else
    return runtime_stats_counter();
#endif
}

static uint32_t MAYBE_UNUSED bench_cycles_to_us_x100(uint32_t cycles)
{
    const uint32_t cycles_per_us = CPU_CLOCK_HZ / 1000000U;
    uint32_t whole_us;
    uint32_t frac_us_x100;

    if (cycles_per_us == 0U) {
        return 0U;
    }

    whole_us = cycles / cycles_per_us;
    frac_us_x100 = ((cycles % cycles_per_us) * 100U) / cycles_per_us;

    return (whole_us * 100U) + frac_us_x100;
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

#if MYOS_TEST_SCENARIO == RTOS_TEST_API_LATENCY
typedef void (*bench_prepare_fn_t)(void);
typedef void (*bench_action_fn_t)(void);
typedef void (*bench_cleanup_fn_t)(void);

static int32_t bench_queue_item;
static void *bench_ptr;

static void bench_noop(void)
{
}

static void bench_empty_action(void)
{
    bench_sink++;
}

static void bench_sem_wait_prepare(void)
{
    (void)sem_init(&test_sem, 1, 1);
}

static void bench_sem_wait_action(void)
{
    (void)sem_wait_timeout(&test_sem, 0);
}

static void bench_sem_signal_prepare(void)
{
    (void)sem_init(&test_sem, 0, 1);
}

static void bench_sem_signal_action(void)
{
    sem_signal(&test_sem);
}

static void bench_mutex_lock_prepare(void)
{
    mutex_init(&test_mutex);
}

static void bench_mutex_lock_action(void)
{
    (void)mutex_lock_timeout(&test_mutex, 0);
}

static void bench_mutex_lock_cleanup(void)
{
    mutex_unlock(&test_mutex);
}

static void bench_mutex_unlock_prepare(void)
{
    mutex_init(&test_mutex);
    (void)mutex_lock_timeout(&test_mutex, 0);
}

static void bench_mutex_unlock_action(void)
{
    mutex_unlock(&test_mutex);
}

static void bench_queue_send_prepare(void)
{
    bench_queue_item++;
}

static void bench_queue_send_action(void)
{
    (void)msg_queue_send_timeout(&test_queue, &bench_queue_item, 0);
}

static void bench_queue_send_cleanup(void)
{
    int32_t item;
    (void)msg_queue_receive_timeout(&test_queue, &item, 0);
}

static void bench_queue_receive_prepare(void)
{
    bench_queue_item++;
    (void)msg_queue_send_timeout(&test_queue, &bench_queue_item, 0);
}

static void bench_queue_receive_action(void)
{
    int32_t item;
    (void)msg_queue_receive_timeout(&test_queue, &item, 0);
}

static void bench_malloc_action(void)
{
    bench_ptr = os_malloc(32U);
}

static void bench_malloc_cleanup(void)
{
    os_free(bench_ptr);
    bench_ptr = NULL;
}

static void bench_free_prepare(void)
{
    bench_ptr = os_malloc(32U);
}

static void bench_free_action(void)
{
    os_free(bench_ptr);
    bench_ptr = NULL;
}

static void bench_print_result(const char *name,
                               uint32_t min_cycles,
                               uint32_t max_cycles,
                               uint32_t avg_cycles)
{
    uint32_t avg_us_x100 = bench_cycles_to_us_x100(avg_cycles);

    uart_print("[RTOS_BENCH] ");
    uart_print(name);
    print_u32(" min=", min_cycles);
    print_u32(" max=", max_cycles);
    print_u32(" avg=", avg_cycles);
    uart_print(" avg_us=");
    print_fixed_x100(avg_us_x100);
    uart_print("\r\n");
}

static void bench_measure(const char *name,
                          bench_prepare_fn_t prepare,
                          bench_action_fn_t action,
                          bench_cleanup_fn_t cleanup)
{
    uint32_t min_cycles = UINT32_MAX;
    uint32_t max_cycles = 0U;
    uint64_t sum_cycles = 0U;

    for (uint32_t i = 0; i < RTOS_BENCH_ITERATIONS; i++) {
        uint32_t start;
        uint32_t end;
        uint32_t delta;

        prepare();
        start = bench_cycles_now();
        action();
        end = bench_cycles_now();
        cleanup();

        delta = end - start;
        if (delta < min_cycles) {
            min_cycles = delta;
        }
        if (delta > max_cycles) {
            max_cycles = delta;
        }
        sum_cycles += delta;
    }

    bench_print_result(name, min_cycles, max_cycles,
                       (uint32_t)(sum_cycles / RTOS_BENCH_ITERATIONS));
}

static void api_latency_bench_task(void)
{
    bench_dwt_init();

    uart_print("[RTOS_BENCH] api_latency iterations=");
    uart_print_dec(RTOS_BENCH_ITERATIONS);
    uart_print(" cpu_hz=");
    uart_print_dec(CPU_CLOCK_HZ);
    uart_print(" source=");
#if MYOS_BENCH_USE_DWT
    uart_print("DWT_CYCCNT\r\n");
#else
    uart_print("SYSTICK_COUNTER\r\n");
#endif

    bench_measure("empty", bench_noop, bench_empty_action, bench_noop);
    bench_measure("sem_wait_ok", bench_sem_wait_prepare,
                  bench_sem_wait_action, bench_noop);
    bench_measure("sem_signal", bench_sem_signal_prepare,
                  bench_sem_signal_action, bench_noop);
    bench_measure("mutex_lock", bench_mutex_lock_prepare,
                  bench_mutex_lock_action, bench_mutex_lock_cleanup);
    bench_measure("mutex_unlock", bench_mutex_unlock_prepare,
                  bench_mutex_unlock_action, bench_noop);
    bench_measure("queue_send", bench_queue_send_prepare,
                  bench_queue_send_action, bench_queue_send_cleanup);
    bench_measure("queue_receive", bench_queue_receive_prepare,
                  bench_queue_receive_action, bench_noop);
    bench_measure("malloc_32", bench_noop, bench_malloc_action,
                  bench_malloc_cleanup);
    bench_measure("free_32", bench_free_prepare, bench_free_action,
                  bench_noop);

    finish_test(1, "api_latency");
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_CONTEXT_SWITCH
static void context_switch_print_result(const char *name,
                                        uint32_t min_cycles,
                                        uint32_t max_cycles,
                                        uint32_t avg_cycles)
{
    uint32_t avg_us_x100 = bench_cycles_to_us_x100(avg_cycles);

    uart_print("[RTOS_BENCH] ");
    uart_print(name);
    print_u32(" min=", min_cycles);
    print_u32(" max=", max_cycles);
    print_u32(" avg=", avg_cycles);
    uart_print(" avg_us=");
    print_fixed_x100(avg_us_x100);
    uart_print("\r\n");
}

static void context_switch_task_a(void)
{
    bench_dwt_init();
    task_delay(1);

    for (uint32_t i = 0; i < RTOS_BENCH_ITERATIONS; i++) {
        while (cs_phase != 0U) {
            task_yield();
        }

        cs_start_cycles = bench_cycles_now();
        cs_phase = 1U;
        task_yield();
    }

    idle_forever();
}

static void context_switch_task_b(void)
{
    uart_print("[RTOS_BENCH] context_switch iterations=");
    uart_print_dec(RTOS_BENCH_ITERATIONS);
    uart_print(" cpu_hz=");
    uart_print_dec(CPU_CLOCK_HZ);
    uart_print(" source=");
#if MYOS_BENCH_USE_DWT
    uart_print("DWT_CYCCNT\r\n");
#else
    uart_print("SYSTICK_COUNTER\r\n");
#endif

    while (cs_samples < RTOS_BENCH_ITERATIONS) {
        if (cs_phase == 1U) {
            uint32_t end = bench_cycles_now();
            uint32_t delta = end - cs_start_cycles;

            if (delta < cs_min_cycles) {
                cs_min_cycles = delta;
            }
            if (delta > cs_max_cycles) {
                cs_max_cycles = delta;
            }
            cs_sum_cycles += delta;
            cs_samples++;
            cs_phase = 0U;
        }

        task_yield();
    }

    context_switch_print_result(
        "yield_switch",
        cs_min_cycles,
        cs_max_cycles,
        cs_sum_cycles / RTOS_BENCH_ITERATIONS);
    finish_test(cs_samples == RTOS_BENCH_ITERATIONS,
                "context_switch_latency");
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_TIMER_JITTER
static void timer_jitter_task(void)
{
    const uint32_t expected_cycles =
        (CPU_CLOCK_HZ / SYSTICK_RATE_HZ) * RTOS_BENCH_PERIOD_TICKS;
    uint32_t previous;
    uint32_t min_period = UINT32_MAX;
    uint32_t max_period = 0U;
    uint32_t max_jitter = 0U;
    uint32_t sum_period = 0U;

    bench_dwt_init();

    uart_print("[RTOS_BENCH] timer_jitter iterations=");
    uart_print_dec(RTOS_BENCH_ITERATIONS);
    uart_print(" period_ticks=");
    uart_print_dec(RTOS_BENCH_PERIOD_TICKS);
    uart_print(" expected_cycles=");
    uart_print_dec(expected_cycles);
    uart_print(" source=");
#if MYOS_BENCH_USE_DWT
    uart_print("DWT_CYCCNT\r\n");
#else
    uart_print("SYSTICK_COUNTER\r\n");
#endif

    task_delay(RTOS_BENCH_PERIOD_TICKS);
    previous = bench_cycles_now();

    for (uint32_t i = 0; i < RTOS_BENCH_ITERATIONS; i++) {
        uint32_t now;
        uint32_t period;
        uint32_t jitter;

        task_delay(RTOS_BENCH_PERIOD_TICKS);
        now = bench_cycles_now();
        period = now - previous;
        previous = now;

        jitter = period > expected_cycles
            ? period - expected_cycles
            : expected_cycles - period;

        if (period < min_period) {
            min_period = period;
        }
        if (period > max_period) {
            max_period = period;
        }
        if (jitter > max_jitter) {
            max_jitter = jitter;
        }
        sum_period += period;
    }

    uart_print("[RTOS_BENCH] task_period");
    print_u32(" min=", min_period);
    print_u32(" max=", max_period);
    print_u32(" avg=", sum_period / RTOS_BENCH_ITERATIONS);
    print_u32(" jitter_max=", max_jitter);
    uart_print(" jitter_us=");
    print_fixed_x100(bench_cycles_to_us_x100(max_jitter));
    uart_print("\r\n");

    finish_test(max_jitter < expected_cycles, "timer_jitter");
    idle_forever();
}
#endif

#if MYOS_TEST_SCENARIO == RTOS_TEST_CPU_LOAD
static void cpu_load_busy_task(void)
{
    while (!test_done) {
        cpu_load_busy_count++;
        bench_sink = cpu_load_busy_count;
    }

    idle_forever();
}

static void cpu_load_check_task(void)
{
    uint32_t start_cycles;
    uint32_t end_cycles;
    uint32_t duration_cycles;

    bench_dwt_init();

    uart_print("[RTOS_BENCH] cpu_load duration_ticks=");
    uart_print_dec(RTOS_BENCH_LOAD_TICKS);
    uart_print(" source=");
#if MYOS_BENCH_USE_DWT
    uart_print("DWT_CYCCNT\r\n");
#else
    uart_print("SYSTICK_COUNTER\r\n");
#endif

    cpu_load_busy_count = 0U;
    start_cycles = bench_cycles_now();
    task_delay(RTOS_BENCH_LOAD_TICKS);
    end_cycles = bench_cycles_now();
    duration_cycles = end_cycles - start_cycles;

    uart_print("[RTOS_BENCH] cpu_load");
    print_u32(" duration_cycles=", duration_cycles);
    uart_print(" duration_us=");
    print_fixed_x100(bench_cycles_to_us_x100(duration_cycles));
    print_u32(" busy_iterations=", cpu_load_busy_count);
    uart_print("\r\n");

    runtime_stats_print();

    finish_test(cpu_load_busy_count > 0U, "cpu_load");
    idle_forever();
}
#endif

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

#if MYOS_TEST_SCENARIO == RTOS_TEST_QUEUE_ISR
static void queue_isr_receiver_task(void)
{
    int32_t data = 0;
    os_status_t status = msg_queue_receive_timeout(&test_queue, &data, 20);

    print_u32("[RTOS_TEST] queue_isr_recv=", (uint32_t)status);
    print_u32(" data=", (uint32_t)data);
    uart_print("\r\n");

    finish_test(status == OS_OK && data == 4321, "queue_from_isr");
    idle_forever();
}

static void queue_isr_sender_task(void)
{
    int32_t data = 4321;
    os_status_t status;

    task_delay(3);
    status = msg_queue_send_from_isr(&test_queue, &data);
    print_u32("[RTOS_TEST] queue_isr_send=", (uint32_t)status);
    uart_print("\r\n");
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

    init_status = sem_init(&test_sem, 0, 3);

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
        ? sem_init(&test_sem, 2, 2)
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

#if MYOS_TEST_SCENARIO == RTOS_TEST_SOFTWARE_TIMER
static void software_timer_oneshot_cb(void *arg)
{
    (void)arg;
    timer_oneshot_count++;
}

static void software_timer_periodic_cb(void *arg)
{
    (void)arg;
    timer_periodic_count++;
}

static void software_timer_check_task(void)
{
    uint32_t periodic_after_stop;

    os_timer_init(&test_oneshot_timer, software_timer_oneshot_cb, NULL);
    os_timer_init(&test_periodic_timer, software_timer_periodic_cb, NULL);

    if (os_timer_start(&test_oneshot_timer, 3U) != OS_OK) {
        finish_test(0, "software_timer");
        idle_forever();
    }

    if (os_timer_start_periodic(&test_periodic_timer, 2U) != OS_OK) {
        finish_test(0, "software_timer");
        idle_forever();
    }

    task_delay(7);
    os_timer_stop(&test_periodic_timer);
    periodic_after_stop = timer_periodic_count;
    task_delay(4);

    print_u32("[RTOS_TEST] oneshot_count=", timer_oneshot_count);
    print_u32(" periodic_count=", timer_periodic_count);
    uart_print("\r\n");

    finish_test(timer_oneshot_count == 1U &&
                periodic_after_stop >= 3U &&
                periodic_after_stop <= 4U &&
                timer_periodic_count == periodic_after_stop,
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
    timer_oneshot_count = 0U;
    timer_periodic_count = 0U;
    cs_phase = 0U;
    cs_start_cycles = 0U;
    cs_samples = 0U;
    cs_min_cycles = UINT32_MAX;
    cs_max_cycles = 0U;
    cs_sum_cycles = 0U;
    cpu_load_busy_count = 0U;

    binary_sem_init(&test_sem, 0);
    mutex_init(&test_mutex);
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
#elif MYOS_TEST_SCENARIO == RTOS_TEST_QUEUE_TIMEOUT
    task_create(queue_timeout_task, PRIORITY_NORMAL);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_ISR_SEMAPHORE
    task_create(isr_sem_waiter_task, PRIORITY_NORMAL);
    task_create(isr_sem_signal_task, PRIORITY_HIGH);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_QUEUE_ISR
    task_create(queue_isr_receiver_task, PRIORITY_NORMAL);
    task_create(queue_isr_sender_task, PRIORITY_HIGH);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_BINARY_SEMAPHORE
    task_create(binary_sem_cap_task, PRIORITY_NORMAL);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_COUNTING_SEMAPHORE
    task_create(counting_sem_task, PRIORITY_NORMAL);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_SOFTWARE_TIMER
    task_create(software_timer_check_task, PRIORITY_NORMAL);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_API_LATENCY
    task_create(api_latency_bench_task, PRIORITY_NORMAL);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_CONTEXT_SWITCH
    task_create(context_switch_task_a, PRIORITY_NORMAL);
    task_create(context_switch_task_b, PRIORITY_NORMAL);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_TIMER_JITTER
    task_create(timer_jitter_task, PRIORITY_NORMAL);
#elif MYOS_TEST_SCENARIO == RTOS_TEST_CPU_LOAD
    {
        uint32_t busy_tid = TASK_INVALID_TID;
        uint32_t check_tid = TASK_INVALID_TID;
        os_status_t busy_status;
        os_status_t check_status;

        busy_status = task_create_dynamic(cpu_load_busy_task, PRIORITY_NORMAL,
                                          OS_DEFAULT_STACK_WORDS, &busy_tid);
        check_status = task_create_dynamic(cpu_load_check_task, PRIORITY_HIGH,
                                           OS_DEFAULT_STACK_WORDS, &check_tid);
        if (busy_status != OS_OK || check_status != OS_OK) {
            finish_test(0, "cpu_load_create");
            return;
        }
        task_set_name(busy_tid, "cpu_busy");
        task_set_name(check_tid, "cpu_check");
    }
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
