#include "mpu.h"
#include "kernel_config.h"
#include <stddef.h>

#define MPU_BASE_ADDR           0xE000ED90UL
#define SCB_SHCSR               (*(volatile uint32_t *)0xE000ED24UL)
#define SCB_SHCSR_MEMFAULTENA   (1U << 16)

typedef struct {
    volatile uint32_t TYPE;
    volatile uint32_t CTRL;
    volatile uint32_t RNR;
    volatile uint32_t RBAR;
    volatile uint32_t RASR;
} MPU_Type;

#define MPU                     ((MPU_Type *)MPU_BASE_ADDR)

#define MPU_CTRL_ENABLE         (1U << 0)
#define MPU_CTRL_HFNMIENA       (1U << 1)
#define MPU_CTRL_PRIVDEFENA     (1U << 2)
#define MPU_RBAR_VALID          (1U << 4)

#ifndef MPU_ENABLE_PRIVILEGED_DEFAULT
#define MPU_ENABLE_PRIVILEGED_DEFAULT 1
#endif

const mpu_region_t static_regions[MPU_STATIC_REGION_COUNT] = {
    [MPU_SLOT_KERNEL_FLASH] = {
        .base       = 0x08000000UL,
        .size_bytes = 64U * 1024U,
        .rasr       = MPU_AP_PRIV_RO | MPU_MEM_NORMAL_WT,
    },

    [MPU_SLOT_KERNEL_RAM] = {
        .base       = 0x20000000UL,
        .size_bytes = 32U * 1024U,
        .rasr       = MPU_AP_PRIV_RW | MPU_MEM_NORMAL_WB_WA | MPU_XN,
    },

    [MPU_SLOT_PERIPH] = {
        .base       = 0x40000000UL,
        .size_bytes = 512U * 1024U * 1024U,
        .rasr       = MPU_AP_PRIV_RW | MPU_MEM_DEVICE_NS | MPU_XN,
    },

    [MPU_SLOT_USER_FLASH] = {
        .base       = 0x08000000UL,
        .size_bytes = 512U * 1024U,
        .rasr       = MPU_AP_RO | MPU_MEM_NORMAL_WT | MPU_SRD(0x01),
    },

    [MPU_SLOT_USER_RAM] = {
        .base       = 0x20000000UL,
        .size_bytes = 128U * 1024U,
        .rasr       = MPU_AP_FULL | MPU_MEM_NORMAL_WB_WA | MPU_XN
                    | MPU_SRD(0x83),
    },

    [MPU_SLOT_GUARD] = {
        .base       = 0x00000000UL,
        .size_bytes = 32U,
        .rasr       = MPU_AP_NOACCESS | MPU_MEM_NORMAL_NC | MPU_XN,
    },
};

uint32_t mpu_encode_size(uint32_t size_bytes)
{
    if (size_bytes < 32U) {
        return 0U;
    }

    if ((size_bytes & (size_bytes - 1U)) != 0U) {
        return 0U;
    }

    uint32_t exp = 0U;
    uint32_t n = size_bytes;

    while (n > 1U) {
        n >>= 1U;
        exp++;
    }

    return (exp - 1U) << 1U;
}

static int mpu_region_is_valid(const mpu_region_t *region)
{
    if (region == NULL) {
        return 0;
    }

    if (region->size_bytes == 0U) {
        return 0;
    }

    if (mpu_encode_size(region->size_bytes) == 0U) {
        return 0;
    }

    if ((region->base & (region->size_bytes - 1U)) != 0U) {
        return 0;
    }

    return 1;
}

static void mpu_write_region(uint8_t slot, const mpu_region_t *region, uint32_t enable)
{
    uint32_t base = 0U;

    if (region != NULL) {
        base = region->base;
    }

    MPU->RBAR = (base & ~0x1FUL) | MPU_RBAR_VALID | (uint32_t)slot;

    if (enable != 0U && mpu_region_is_valid(region)) {
        uint32_t size = mpu_encode_size(region->size_bytes);
        MPU->RASR = region->rasr | size | MPU_REGION_ENABLE;
    } else {
        MPU->RASR = MPU_REGION_DISABLE;
    }
}

void mpu_init(void)
{
    static uint8_t initialized = 0U;

    if (initialized != 0U) {
        return;
    }

    MPU->CTRL = 0U;

    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");

    for (uint8_t i = 0U; i < MPU_STATIC_REGION_COUNT; i++) {
        mpu_write_region(i, &static_regions[i], 1U);
    }

    MPU->RNR = MPU_SLOT_TASK_STACK;
    MPU->RASR = MPU_REGION_DISABLE;
    mpu_disable_extra();

    SCB_SHCSR |= SCB_SHCSR_MEMFAULTENA;

    MPU->CTRL = MPU_CTRL_ENABLE | MPU_CTRL_HFNMIENA
#if MPU_ENABLE_PRIVILEGED_DEFAULT
              | MPU_CTRL_PRIVDEFENA
#endif
              ;

    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");

    initialized = 1U;
}

void mpu_switch_task(const task_mpu_t *task_mpu)
{
    if (task_mpu == NULL) {
        return;
    }

    mpu_write_region(MPU_SLOT_TASK_STACK, &task_mpu->stack_region, 1U);

    if (task_mpu->extra_region.size_bytes != 0U) {
        mpu_write_region(MPU_SLOT_TASK_EXTRA, &task_mpu->extra_region, 1U);
    } else {
        mpu_disable_extra();
    }

    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");
}

void mpu_disable_extra(void)
{
    MPU->RBAR = MPU_RBAR_VALID | MPU_SLOT_TASK_EXTRA;
    MPU->RASR = MPU_REGION_DISABLE;
}
