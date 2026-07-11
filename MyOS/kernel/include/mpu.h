#ifndef MPU_H
#define MPU_H

/*
 * mpu.h — MPU driver interface for STM32F407 (Cortex-M4)
 *
 * Responsibilities:
 *   mpu_init()          — program static regions 0–5 once at boot
 *   mpu_switch_task()   — update dynamic regions 6–7 on context switch
 *   mpu_disable_extra() — clear slot 7 when task has no extra region
 *
 * Call sequence:
 *   Board init:     mpu_init()
 *   Context switch: mpu_switch_task(&next_task->mpu)
 */

#include "memory_layout.h"

/* =========================================================
 * BOOT — program static regions and enable MPU
 *
 * Programs slots 0–5 from static_regions[] table.
 * Enables MPU with PRIVDEFENA controlled by
 * MPU_ENABLE_PRIVILEGED_DEFAULT in kernel_config.h.
 *
 * Call once from board_init(), before kernel_init().
 * ========================================================= */
void mpu_init(void);

/* =========================================================
 * CONTEXT SWITCH — update dynamic regions 6 and 7
 *
 * Call from PendSV_Handler (or port layer) with the incoming
 * task's mpu settings. Writes directly to MPU registers while
 * interrupts are disabled (caller's responsibility).
 *
 *  task_mpu->stack_region → slot 6  (always enabled)
 *  task_mpu->extra_region → slot 7  (disabled if size_bytes == 0)
 *
 * Example (FreeRTOS port hook):
 *   void vPortStoreTaskMPUSettings(...) {
 *       task->mpu.stack_region.base       = (uint32_t)pxBottomOfStack;
 *       task->mpu.stack_region.size_bytes = ulStackDepth * 4;
 *       task->mpu.stack_region.rasr       = MPU_AP_FULL
 *                                         | MPU_MEM_NORMAL_WB_WA
 *                                         | MPU_XN;
 *       task->mpu.extra_region.size_bytes = 0; // disabled
 *   }
 * ========================================================= */
void mpu_switch_task(const task_mpu_t *task_mpu);

/* =========================================================
 * HELPERS
 * ========================================================= */

/* Disable slot 7 (call when leaving a task that had extra region) */
void mpu_disable_extra(void);

/* Convert a power-of-2 byte size to RASR SIZE field [7:1].
 * Returns 0 if size is invalid (< 32 or not power of 2).
 * Used internally — exposed for unit testing. */
uint32_t mpu_encode_size(uint32_t size_bytes);

#endif /* MPU_H */
