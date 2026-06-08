#ifndef MEMORY_LAYOUT_H
#define MEMORY_LAYOUT_H

/*
 * memory_layout.h — MPU region layout for STM32F407 (Cortex-M4)
 *
 * Region map (Cortex-M4 has 8 hardware slots: 0–7):
 *
 *  Slot  Type     Content
 *  ────  ───────  ──────────────────────────────────────────
 *   0    STATIC   Kernel Flash   (priv RO, exec)
 *   1    STATIC   Kernel RAM     (priv RW, no exec)
 *   2    STATIC   Peripherals    (priv RW, XN, device)
 *   3    STATIC   User Flash     (user RO, exec)
 *   4    STATIC   User RAM global(user RW, no exec)
 *   5    STATIC   Guard / NULL   (no access — catches NULL deref
 *                                 and stack overflow sentinel)
 *   6    DYNAMIC  Current task stack   (updated every context switch)
 *   7    DYNAMIC  Current task extra   (optional private memory)
 *
 * Reference: ARM DDI0403E, ST RM0090 Rev 19 §8
 */

#include <stdint.h>

/* =========================================================
 * REGION SLOT INDICES
 * ========================================================= */
#define MPU_SLOT_KERNEL_FLASH    0U
#define MPU_SLOT_KERNEL_RAM      1U
#define MPU_SLOT_PERIPH          2U
#define MPU_SLOT_USER_FLASH      3U
#define MPU_SLOT_USER_RAM        4U
#define MPU_SLOT_GUARD           5U
#define MPU_SLOT_TASK_STACK      6U   /* dynamic — per context switch */
#define MPU_SLOT_TASK_EXTRA      7U   /* dynamic — per context switch */

#define MPU_STATIC_REGION_COUNT  6U   /* slots 0–5 */
#define MPU_DYNAMIC_SLOTS        2U   /* slots 6–7 */
#define MPU_MAX_REGIONS          8U   /* Cortex-M4 hardware limit    */

#if (MPU_STATIC_REGION_COUNT + MPU_DYNAMIC_SLOTS) != MPU_MAX_REGIONS
#error "Static + dynamic slots must equal MPU_MAX_REGIONS (8)"
#endif

/* =========================================================
 * MPU RASR ATTRIBUTE MACROS
 *
 * Pass OR-combination as `rasr` field of mpu_region_t.
 * mpu_configure() inserts SIZE bits and sets EN automatically.
 *
 * RASR [31:0]:
 *   [28]    XN      Execute Never
 *   [26:24] AP      Access Permission
 *   [21:19] TEX     Type Extension
 *   [18]    S       Shareable
 *   [17]    C       Cacheable
 *   [16]    B       Bufferable
 *   [15:8]  SRD     Subregion Disable
 *   [7:1]   SIZE    region_bytes = 2^(SIZE+1), SIZE >= 4
 *   [0]     EN      Region Enable
 * ========================================================= */

/* --- Access Permission AP [26:24] ---------------------------------- */
#define MPU_AP_NOACCESS         (0x0U << 24)  /* P: ---  U: ---        */
#define MPU_AP_PRIV_RW          (0x1U << 24)  /* P: RW   U: ---        */
#define MPU_AP_PRIV_RW_USER_RO  (0x2U << 24)  /* P: RW   U: RO         */
#define MPU_AP_FULL             (0x3U << 24)  /* P: RW   U: RW         */
#define MPU_AP_PRIV_RO          (0x5U << 24)  /* P: RO   U: ---        */
#define MPU_AP_RO               (0x6U << 24)  /* P: RO   U: RO         */

/* --- Execute Never XN [28] ----------------------------------------- */
#define MPU_XN                  (1U << 28)

/* --- Memory type: TEX[21:19] S[18] C[17] B[16] --------------------- *
 * From ARM DDI0403E Table B3-55:                                        *
 *                                                                       *
 *  Macro                  TEX  S  C  B   Description                   *
 *  MPU_MEM_SO             000  0  0  0   Strongly-ordered              *
 *  MPU_MEM_DEVICE_S       000  1  0  0   Device, shareable             *
 *  MPU_MEM_DEVICE_NS      010  0  0  0   Device, non-shareable         *
 *  MPU_MEM_NORMAL_WT      000  0  1  0   Normal, write-through         *
 *  MPU_MEM_NORMAL_WB      000  0  1  1   Normal, write-back            *
 *  MPU_MEM_NORMAL_NC      001  0  0  0   Normal, non-cacheable         *
 *  MPU_MEM_NORMAL_WB_WA   001  0  1  1   Normal, WB + write-alloc      *
 * --------------------------------------------------------------------- */
#define MPU_MEM_SO              (0x00U << 16)
#define MPU_MEM_DEVICE_S        ((0x00U << 19) | (1U << 18))
#define MPU_MEM_DEVICE_NS       (0x02U << 19)
#define MPU_MEM_NORMAL_WT       ((0x00U << 19) | (1U << 17))
#define MPU_MEM_NORMAL_WB       ((0x00U << 19) | (1U << 17) | (1U << 16))
#define MPU_MEM_NORMAL_NC       (0x01U << 19)
#define MPU_MEM_NORMAL_WB_WA    ((0x01U << 19) | (1U << 17) | (1U << 16))
#define MPU_SHARED              (1U << 18)

/* --- Subregion Disable SRD [15:8] ---------------------------------- */
#define MPU_SRD_NONE            (0x00U << 8)
#define MPU_SRD(mask)           (((mask) & 0xFFU) << 8)

/* --- SIZE encoding [7:1] ------------------------------------------- *
 * SIZE = log2(region_bytes) - 1  →  region_bytes = 2^(SIZE+1)         *
 * Minimum SIZE = 4 (32 bytes minimum region)                           *
 * -------------------------------------------------------------------- */
#define MPU_SIZE_32B            (0x04U << 1)
#define MPU_SIZE_64B            (0x05U << 1)
#define MPU_SIZE_128B           (0x06U << 1)
#define MPU_SIZE_256B           (0x07U << 1)
#define MPU_SIZE_512B           (0x08U << 1)
#define MPU_SIZE_1K             (0x09U << 1)
#define MPU_SIZE_2K             (0x0AU << 1)
#define MPU_SIZE_4K             (0x0BU << 1)
#define MPU_SIZE_8K             (0x0CU << 1)
#define MPU_SIZE_16K            (0x0DU << 1)
#define MPU_SIZE_32K            (0x0EU << 1)
#define MPU_SIZE_64K            (0x0FU << 1)
#define MPU_SIZE_128K           (0x10U << 1)
#define MPU_SIZE_256K           (0x11U << 1)
#define MPU_SIZE_512K           (0x12U << 1)
#define MPU_SIZE_1M             (0x13U << 1)

/* --- Region Enable EN [0] ------------------------------------------ */
#define MPU_REGION_ENABLE       (1U << 0)
#define MPU_REGION_DISABLE      (0U << 0)

/* =========================================================
 * MPU REGION DESCRIPTOR
 *
 *  base       : must be aligned to size_bytes (hardware requirement)
 *  size_bytes : must be power of 2, >= 32 bytes
 *  rasr       : attribute bits from macros above;
 *               SIZE and EN are inserted by mpu_configure()
 * ========================================================= */
typedef struct
{
    uint32_t base;          /* Base address (size-aligned)                  */
    uint32_t size_bytes;    /* Size in bytes (power of 2, >= 32)            */
    uint32_t rasr;          /* Attribute bits — SIZE/EN set by driver       */
} mpu_region_t;

/* =========================================================
 * PER-TASK DYNAMIC MPU SETTINGS
 *
 * Stored in each task's TCB (or alongside it).
 * Loaded into slots 6–7 on every context switch.
 *
 * stack_region:
 *   base       = bottom of task stack (grows upward to base+size)
 *   size_bytes = stack size (must be power of 2)
 *   rasr       = MPU_AP_FULL | MPU_MEM_NORMAL_WB_WA | MPU_XN
 *
 * extra_region:
 *   Optional — task-private memory region.
 *   Set size_bytes = 0 and rasr = MPU_REGION_DISABLE to leave slot 7 off.
 * ========================================================= */
typedef struct
{
    mpu_region_t stack_region;  /* slot 6 — task stack                      */
    mpu_region_t extra_region;  /* slot 7 — task extra (0 = disabled)       */
} task_mpu_t;

/* =========================================================
 * STATIC REGION TABLE  (defined in memory_layout.c)
 * Indexed by MPU_SLOT_* constants 0–5.
 * ========================================================= */
extern const mpu_region_t static_regions[MPU_STATIC_REGION_COUNT];

/* Convenience alias — total static region count */
#define system_region_count  MPU_STATIC_REGION_COUNT

#endif /* MEMORY_LAYOUT_H */
