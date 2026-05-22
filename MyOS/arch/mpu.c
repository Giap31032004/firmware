/*
 * mpu.c — MPU driver for STM32F407 (Cortex-M4)
 */

#include "mpu.h"
#include <stddef.h>

/* =========================================================
 * CMSIS-compatible MPU register access
 * (avoids pulling in full CMSIS header)
 * ========================================================= */
#define MPU_BASE        (0xE000ED90UL)

typedef struct
{
    volatile uint32_t TYPE;     /* 0x00 — number of regions        */
    volatile uint32_t CTRL;     /* 0x04 — enable / PRIVDEFENA      */
    volatile uint32_t RNR;      /* 0x08 — region number select     */
    volatile uint32_t RBAR;     /* 0x0C — region base address      */
    volatile uint32_t RASR;     /* 0x10 — region attributes + size */
} MPU_Type;

#define MPU             ((MPU_Type *)MPU_BASE)

/* CTRL bits */
#define MPU_CTRL_ENABLE         (1U << 0)
#define MPU_CTRL_HFNMIENA       (1U << 1)   /* MPU active during HardFault/NMI */
#define MPU_CTRL_PRIVDEFENA     (1U << 2)   /* 0 = no default map fallback      */

/* RBAR VALID bit — when set, RNR is updated from RBAR[3:0] atomically */
#define MPU_RBAR_VALID          (1U << 4)

/* =========================================================
 * STATIC REGION TABLE  (slots 0–5)
 *
 * Adjust base/size to match your exact flash/RAM sizes.
 * size_bytes must be a power of 2 and base must be aligned to it.
 * ========================================================= */
const mpu_region_t static_regions[MPU_STATIC_REGION_COUNT] =
{
    /* Slot 0 — Kernel Flash: priv RO, executable, write-through cache */
    [MPU_SLOT_KERNEL_FLASH] = {
        .base       = 0x08000000UL,
        .size_bytes = 64U * 1024U,
        .rasr       = MPU_AP_PRIV_RO | MPU_MEM_NORMAL_WT,
    },

    /* Slot 1 — Kernel RAM: priv RW, no exec, WB+WA cache */
    [MPU_SLOT_KERNEL_RAM] = {
        .base       = 0x20000000UL,
        .size_bytes = 32U * 1024U,
        .rasr       = MPU_AP_PRIV_RW | MPU_MEM_NORMAL_WB_WA | MPU_XN,
    },

    /* Slot 2 — Peripherals: priv RW, XN, device (non-shareable) */
    [MPU_SLOT_PERIPH] = {
        .base       = 0x40000000UL,
        .size_bytes = 512U * 1024U * 1024U,  /* 0x40000000–0x5FFFFFFF */
        .rasr       = MPU_AP_PRIV_RW | MPU_MEM_DEVICE_NS | MPU_XN,
    },

    /* Slot 3 — User Flash: user RO, executable, write-through cache */
    [MPU_SLOT_USER_FLASH] = {
        .base       = 0x08010000UL,
        .size_bytes = 448U * 1024U,
        /* 448K is not a power of 2 — use 512K region + SRD to mask top 64K */
        /* SRD bit 7 disables the last 1/8 (64K) sub-region                 */
        .rasr       = MPU_AP_RO | MPU_MEM_NORMAL_WT | MPU_SRD(0x80),
    },

    /* Slot 4 — User RAM global: user RW, no exec, WB+WA cache */
    [MPU_SLOT_USER_RAM] = {
        .base       = 0x20008000UL,
        .size_bytes = 128U * 1024U,
        /* 80K is not a power of 2 — use 128K + SRD to mask top 48K (3/8)  */
        /* SRD bits [7:5] = 0b111 → disable sub-regions 5,6,7 (48K)        */
        .rasr       = MPU_AP_FULL | MPU_MEM_NORMAL_WB_WA | MPU_XN
                    | MPU_SRD(0xE0),
    },

    /* Slot 5 — Guard / NULL trap: no access, catches NULL deref + stack OVF */
    [MPU_SLOT_GUARD] = {
        .base       = 0x00000000UL,
        .size_bytes = 32U,              /* minimum 32-byte region            */
        .rasr       = MPU_AP_NOACCESS | MPU_MEM_NORMAL_NC | MPU_XN,
    },
};

/* =========================================================
 * INTERNAL: encode size_bytes → RASR SIZE field [7:1]
 * SIZE = log2(size_bytes) - 1
 * ========================================================= */
uint32_t mpu_encode_size(uint32_t size_bytes)
{
    if (size_bytes < 32U) { return 0U; }

    /* check power of 2 */
    if ((size_bytes & (size_bytes - 1U)) != 0U) { return 0U; }

    uint32_t exp = 0U;
    uint32_t n   = size_bytes;
    while (n > 1U) { n >>= 1U; exp++; }

    /* SIZE = exp - 1, shift into [7:1] */
    return ((exp - 1U) << 1U);
}

/* =========================================================
 * INTERNAL: write one region to hardware
 * ========================================================= */
static void write_region(uint8_t slot, const mpu_region_t *r, uint32_t en)
{
    uint32_t size_enc = mpu_encode_size(r->size_bytes);

    /* Write RBAR with VALID bit to atomically select region */
    MPU->RBAR = (r->base & ~0x1FUL) | MPU_RBAR_VALID | (uint32_t)slot;

    if (en && size_enc != 0U)
    {
        MPU->RASR = r->rasr | size_enc | MPU_REGION_ENABLE;
    }
    else
    {
        MPU->RASR = MPU_REGION_DISABLE;
    }
}

/* =========================================================
 * mpu_init — program static regions 0–5, enable MPU
 * Call once at boot (after SystemInit, before main)
 * ========================================================= */
void mpu_init(void)
{
    /* Disable MPU while configuring */
    MPU->CTRL = 0U;

    /* Memory barrier — ensure MPU is off before writing regions */
    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");

    /* Program static regions 0–5 */
    for (uint8_t i = 0U; i < MPU_STATIC_REGION_COUNT; i++)
    {
        write_region(i, &static_regions[i], 1U);
    }

    /* Disable dynamic slots 6–7 until first context switch */
    mpu_disable_extra();
    MPU->RNR  = MPU_SLOT_TASK_STACK;
    MPU->RASR = MPU_REGION_DISABLE;

    /* Enable MPU:
     *   HFNMIENA = 1 → MPU active even in HardFault/NMI (safer)
     *   PRIVDEFENA = 0 → no default map, privileged faults on unmapped access
     */
    MPU->CTRL = MPU_CTRL_ENABLE | MPU_CTRL_HFNMIENA;

    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");
}

/* =========================================================
 * mpu_switch_task — update slots 6–7 on context switch
 * Must be called with interrupts disabled (inside PendSV)
 * ========================================================= */
void mpu_switch_task(const task_mpu_t *task_mpu)
{
    /* Slot 6 — task stack (always enabled) */
    write_region(MPU_SLOT_TASK_STACK, &task_mpu->stack_region, 1U);

    /* Slot 7 — task extra (disabled if size_bytes == 0) */
    if (task_mpu->extra_region.size_bytes != 0U)
    {
        write_region(MPU_SLOT_TASK_EXTRA, &task_mpu->extra_region, 1U);
    }
    else
    {
        mpu_disable_extra();
    }

    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");
}

/* =========================================================
 * mpu_disable_extra — clear slot 7
 * ========================================================= */
void mpu_disable_extra(void)
{
    MPU->RBAR = MPU_RBAR_VALID | MPU_SLOT_TASK_EXTRA;
    MPU->RASR = MPU_REGION_DISABLE;
}