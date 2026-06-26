#ifndef KERNEL_CONFIG_H
#define KERNEL_CONFIG_H

/* =========================================================
 *                  TASK / SCHEDULER LIMITS
 * ========================================================= */

#define MAX_TASKS               8
#define MAX_PRIORITY            8

#define OS_USE_PREEMPTION       1
#define OS_USE_TIME_SLICING     1
#define OS_TIME_SLICE_TICKS     1
#define OS_USE_TICKLESS_IDLE    1
#define OS_EXPECTED_IDLE_TIME_BEFORE_SLEEP 2U
#define OS_GENERATE_RUN_TIME_STATS 1
#define OS_USE_TRACE_FACILITY   1
#define OS_TRACE_BUFFER_SIZE    64U
#define OS_DIAG_VERBOSE_LOG     0
#define OS_ENABLE_ASSERT        1
#define OS_ENABLE_STACK_CHECK   1
#define MPU_ENABLE_PRIVILEGED_DEFAULT 1

#if MAX_TASKS == 0
#error "MAX_TASKS must be greater than 0"
#endif

#if MAX_PRIORITY > 32
#error "Bitmap scheduler supports only 32 priorities"
#endif


/* =========================================================
 *                  STACK CONFIGURATION
 * ========================================================= */

/* Stack size unit = WORDS (1 WORD = 4 bytes on Cortex-M) */

#define OS_MIN_STACK_WORDS      64
#define OS_DEFAULT_STACK_WORDS  256

/* Single source of truth */
#define STACK_SIZE              OS_DEFAULT_STACK_WORDS


/* =========================================================
 *                  SAFETY / DEBUG
 * ========================================================= */

#define STACK_CANARY_VALUE      0xDEADBEEF
#define STACK_FILL_VALUE        0xA5A5A5A5U
#define OS_STACK_GUARD_WORDS    4U

#if OS_STACK_GUARD_WORDS == 0
#error "OS_STACK_GUARD_WORDS must be greater than 0"
#endif


#endif /* KERNEL_CONFIG_H */
