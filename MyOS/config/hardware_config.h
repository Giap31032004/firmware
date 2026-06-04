#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

/* =========================================================
 *                  CPU / CLOCK
 * ========================================================= */

#define CPU_CLOCK_HZ            16000000U


/* =========================================================
 *                  SYSTEM TIMER
 * ========================================================= */

/* Kernel tick frequency */
#define SYSTICK_RATE_HZ         1000U


/* =========================================================
 *                  INTERRUPT PRIORITY
 * ========================================================= */

/*
 * Cortex-M IRQ priorities:
 * smaller number = higher interrupt priority.
 *
 * Keep IRQ_PRIO_* separate from TASK_PRIO_* values. Task priorities use
 * larger number = higher priority in the MyOS scheduler.
 */

#define IRQ_PRIO_KERNEL_LOWEST      15
#define IRQ_PRIO_KERNEL_SVC         14

#define IRQ_PRIO_SYSTICK            IRQ_PRIO_KERNEL_LOWEST
#define IRQ_PRIO_PENDSV             IRQ_PRIO_KERNEL_LOWEST
#define IRQ_PRIO_SVC                IRQ_PRIO_KERNEL_SVC

/* Backward-compatible aliases. New code should use IRQ_PRIO_* names. */
#define SYSTICK_IRQ_PRIORITY        IRQ_PRIO_SYSTICK
#define PENDSV_IRQ_PRIORITY         IRQ_PRIO_PENDSV
#define SVC_IRQ_PRIORITY            IRQ_PRIO_SVC


#endif /* HARDWARE_CONFIG_H */
