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
 * Cortex-M:
 * smaller number = higher priority
 */

#define SYSTICK_IRQ_PRIORITY    15
#define PENDSV_IRQ_PRIORITY     15
#define SVC_IRQ_PRIORITY        14


#endif /* HARDWARE_CONFIG_H */