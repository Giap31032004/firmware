#ifndef KERNEL_CONFIG_H
#define KERNEL_CONFIG_H

/* =========================================================
 *                  TASK / SCHEDULER LIMITS
 * ========================================================= */

#define MAX_TASKS               20
#define MAX_PRIORITY            32

#define OS_USE_PREEMPTION       1
#define OS_USE_TIME_SLICING     1
#define OS_TIME_SLICE_TICKS     1
#define OS_ENABLE_ASSERT        1
#define OS_ENABLE_STACK_CHECK   1

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


#endif /* KERNEL_CONFIG_H */
