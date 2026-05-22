#ifndef MPU_REGS_H
#define MPU_REGS_H

#include <stdint.h>

/* =========================================================
 * MPU base address (ARMv7-M)
 * ========================================================= */
#define MPU_BASE        (0xE000ED90UL)

/* =========================================================
 * MPU register map
 * ========================================================= */
typedef struct
{
    volatile uint32_t TYPE;   /* 0x00 */
    volatile uint32_t CTRL;   /* 0x04 */
    volatile uint32_t RNR;    /* 0x08 */
    volatile uint32_t RBAR;   /* 0x0C */
    volatile uint32_t RASR;   /* 0x10 */
} MPU_Type;

/* =========================================================
 * MPU instance
 * ========================================================= */
#define MPU   ((MPU_Type *)MPU_BASE)

/* =========================================================
 * CTRL register bits
 * ========================================================= */
#define MPU_CTRL_ENABLE        (1U << 0)
#define MPU_CTRL_HFNMIENA      (1U << 1)
#define MPU_CTRL_PRIVDEFENA    (1U << 2)

/* =========================================================
 * RBAR bits
 * ========================================================= */
#define MPU_RBAR_ADDR_MASK     (0xFFFFFFE0UL)
#define MPU_RBAR_VALID         (1U << 4)
#define MPU_RBAR_REGION_MASK   (0xFUL)

/* =========================================================
 * RASR bits
 * ========================================================= */
#define MPU_RASR_ENABLE        (1U << 0)
#define MPU_RASR_SIZE_POS      (1U)

/* Execute Never */
#define MPU_XN                 (1U << 28)

/* Access Permission */
#define MPU_AP_POS            (24U)

/* Subregion Disable */
#define MPU_SRD_POS           (8U)

#endif /* MPU_REGS_H */