#include "fault.h"

/* System Control Block (SCB) Registers */
#define SCB_CCR   (*(volatile uint32_t *)0xE000ED14) // Configuration and Control Register
#define SCB_SHCSR (*(volatile uint32_t *)0xE000ED24) // System Handler Control and State Register
#define SCB_CFSR  (*(volatile uint32_t *)0xE000ED28) // Configurable Fault Status Register
#define SCB_HFSR  (*(volatile uint32_t *)0xE000ED2C) // HardFault Status Register
#define SCB_MMAR  (*(volatile uint32_t *)0xE000ED34) // MemManage Fault Address Register
#define SCB_BFAR  (*(volatile uint32_t *)0xE000ED38) // BusFault Address Register

/* * [SỬA LỖI 3]: Loại bỏ printf() tiêu chuẩn.
 * Yêu cầu người dùng định nghĩa một hàm in chuỗi ra UART theo kiểu Polling (không dùng ngắt).
 */
extern void uart_polled_puts(const char *str); 
#define DEBUG_PUTS(str) uart_polled_puts(str)

volatile Fault_Context_t g_last_fault_context __attribute__((used));

// ---------------------------------------------------------
// 1. MODULE DECODE (CFSR & HFSR)
// ---------------------------------------------------------

/* [SỬA LỖI 4]: Thêm bộ giải mã cho HardFault Status Register */
static void fault_decode_hfsr(uint32_t hfsr) {
    DEBUG_PUTS("\n--- HFSR Decode ---\n");
    if (hfsr & (1 << 30)) DEBUG_PUTS("[HardFault] FORCED: Escaped from lower priority fault\n");
    if (hfsr & (1 << 1))  DEBUG_PUTS("[HardFault] VECTTBL: Vector table read fault\n");
}

static void fault_decode_cfsr(uint32_t cfsr) {
    DEBUG_PUTS("\n--- CFSR Decode ---\n");

    /* MemManage Fault */
    if (cfsr & (1 << 0))  DEBUG_PUTS("[MemManage] IACCVIOL: Instruction access violation\n");
    if (cfsr & (1 << 1))  DEBUG_PUTS("[MemManage] DACCVIOL: Data access violation\n");
    if (cfsr & (1 << 3))  DEBUG_PUTS("[MemManage] MUNSTKERR: Unstacking error\n");
    if (cfsr & (1 << 4))  DEBUG_PUTS("[MemManage] MSTKERR: Stacking error\n");

    /* BusFault */
    if (cfsr & (1 << 8))  DEBUG_PUTS("[BusFault] IBUSERR: Instruction bus error\n");
    if (cfsr & (1 << 9))  DEBUG_PUTS("[BusFault] PRECISERR: Precise data bus error\n");
    if (cfsr & (1 << 10)) DEBUG_PUTS("[BusFault] IMPRECISERR: Imprecise data bus error\n");
    if (cfsr & (1 << 11)) DEBUG_PUTS("[BusFault] UNSTKERR: Unstacking error\n");
    if (cfsr & (1 << 12)) DEBUG_PUTS("[BusFault] STKERR: Stacking error\n");

    /* UsageFault */
    if (cfsr & (1 << 16)) DEBUG_PUTS("[UsageFault] UNDEFINSTR: Undefined instruction\n");
    if (cfsr & (1 << 17)) DEBUG_PUTS("[UsageFault] INVSTATE: Invalid state\n");
    if (cfsr & (1 << 18)) DEBUG_PUTS("[UsageFault] INVPC: Invalid PC\n");
    if (cfsr & (1 << 19)) DEBUG_PUTS("[UsageFault] NOCP: No coprocessor\n");
    if (cfsr & (1 << 24)) DEBUG_PUTS("[UsageFault] UNALIGNED: Unaligned access\n");
    if (cfsr & (1 << 25)) DEBUG_PUTS("[UsageFault] DIVBYZERO: Divide by zero\n");
}

// ---------------------------------------------------------
// 2. MODULE RECOVERY / PANIC
// ---------------------------------------------------------
static void fault_recover_or_panic(void) {
    DEBUG_PUTS("\nSystem PANIC! System halted.\n");
    
    /* [SỬA LỖI 7]: Dùng inline assembly chuẩn xác để tắt ngắt (độc lập với CMSIS header) */
    __asm volatile("cpsid i" : : : "memory"); 
    
    while (1) {
        __asm volatile("nop");
    }
}

// ---------------------------------------------------------
// 3. CORE ANALYZER
// ---------------------------------------------------------
void fault_analyzer_c(Fault_StackFrame_t *frame, uint32_t exc_return) {
    g_last_fault_context.stack_frame = frame;
    g_last_fault_context.exc_return  = exc_return;
    g_last_fault_context.cfsr        = SCB_CFSR;
    g_last_fault_context.hfsr        = SCB_HFSR;
    
    /* [SỬA LỖI 1]: Xóa rác mmar và bfar trước khi kiểm tra cờ VALID */
    g_last_fault_context.mmar = 0;
    g_last_fault_context.bfar = 0;

    if (g_last_fault_context.cfsr & (1 << 7))  g_last_fault_context.mmar = SCB_MMAR;
    if (g_last_fault_context.cfsr & (1 << 15)) g_last_fault_context.bfar = SCB_BFAR;

    /* [SỬA LỖI 2]: Xóa sticky flags bằng cách write-1-to-clear */
    SCB_CFSR = g_last_fault_context.cfsr;
    SCB_HFSR = g_last_fault_context.hfsr;

    // In log chẩn đoán
    DEBUG_PUTS("======================================\n");
    DEBUG_PUTS("        HARDWARE FAULT DETECTED       \n");
    DEBUG_PUTS("======================================\n");

    fault_decode_hfsr(g_last_fault_context.hfsr);
    fault_decode_cfsr(g_last_fault_context.cfsr);

    fault_recover_or_panic();
}

// ---------------------------------------------------------
// 4. ASSEMBLY WRAPPERS
// ---------------------------------------------------------
/* * [SỬA LỖI 6 & 5]: Tháo macro ra từng hàm riêng biệt để an toàn tuyệt đối với GCC.
 * Dùng 'bx' kết hợp với thanh ghi thay vì 'b' để đảm bảo an toàn State (Thumb).
 */

__attribute__((naked)) void HardFault_Handler(void) {
    __asm volatile (
        " tst lr, #4 \n"
        " ite eq \n"
        " mrseq r0, msp \n"
        " mrsne r0, psp \n"
        " mov r1, lr \n"
        " ldr r2, =fault_analyzer_c \n"
        " bx r2 \n"
    );
}

__attribute__((naked)) void MemManage_Handler(void) {
    __asm volatile (
        " tst lr, #4 \n"
        " ite eq \n"
        " mrseq r0, msp \n"
        " mrsne r0, psp \n"
        " mov r1, lr \n"
        " ldr r2, =fault_analyzer_c \n"
        " bx r2 \n"
    );
}

__attribute__((naked)) void BusFault_Handler(void) {
    __asm volatile (
        " tst lr, #4 \n"
        " ite eq \n"
        " mrseq r0, msp \n"
        " mrsne r0, psp \n"
        " mov r1, lr \n"
        " ldr r2, =fault_analyzer_c \n"
        " bx r2 \n"
    );
}

__attribute__((naked)) void UsageFault_Handler(void) {
    __asm volatile (
        " tst lr, #4 \n"
        " ite eq \n"
        " mrseq r0, msp \n"
        " mrsne r0, psp \n"
        " mov r1, lr \n"
        " ldr r2, =fault_analyzer_c \n"
        " bx r2 \n"
    );
}

// ---------------------------------------------------------
// 5. INITIALIZATION
// ---------------------------------------------------------
void fault_subsystem_init(void) {
    /* Bật UsageFault, BusFault, và MemManage */
    SCB_SHCSR |= (1 << 18) | (1 << 17) | (1 << 16);
                 
    /* [SỬA LỖI 8]: Bật cờ bắt lỗi chia 0 và Unaligned memory ở tầng phần cứng */
    SCB_CCR |= (1 << 4) | (1 << 3); // DIV_0_TRP và UNALIGN_TRP
}