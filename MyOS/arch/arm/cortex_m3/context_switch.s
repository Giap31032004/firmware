.syntax unified
.cpu cortex-m3
.thumb

.global PendSV_Handler
.global start_first_task

/* External variables from C */
.extern current_pcb
.extern next_pcb

.section .text

/* ========================================
   HÀM: PendSV_Handler
   Mô tả: Thực hiện lưu và khôi phục ngữ cảnh (Context Switch)
   ======================================== */
.type PendSV_Handler, %function
PendSV_Handler:
    /* 1. Lấy PSP hiện tại */
    MRS     r0, psp
    CBZ     r0, load_next_task      /* Nếu PSP=0 (chưa chạy task nào), nhảy đến load luôn */

    /* 2. Lưu Context cũ (R4-R11) */
    LDR     r1, =current_pcb
    LDR     r1, [r1]
    CBZ     r1, load_next_task      /* Safety check: Nếu current_pcb NULL, bỏ qua lưu */

    STMDB   r0!, {r4-r11}           /* Push R4-R11 vào stack */
    STR     r0, [r1]                /* Lưu SP mới vào current_pcb->stack_ptr */

load_next_task:
    /* 3. Lấy Task tiếp theo */
    LDR     r1, =next_pcb
    LDR     r1, [r1]
    CBZ     r1, pend_exit           /* Safety check: Nếu next_pcb NULL, thoát */

    LDR     r0, [r1]                /* r0 = next_pcb->stack_ptr */
    
    /* 4. Cập nhật current_pcb = next_pcb */
    LDR     r2, =current_pcb
    STR     r1, [r2]

    /* 5. Khôi phục Context mới (R4-R11) */
    LDMIA   r0!, {r4-r11}           /* Pop R4-R11 từ stack */
    MSR     psp, r0                 /* Cập nhật thanh ghi PSP */
    
    /* 6. Barriers (Quan trọng) */
    DSB                             /* Data Synchronization Barrier */
    ISB                             /* Instruction Synchronization Barrier */

pend_exit:
    /* 7. Thoát ngắt */
    ORR     lr, lr, #0x04           /* Đảm bảo Bit 2 = 1 (Return to Thread Mode, use PSP) */
    BX      lr

/* ========================================
   HÀM: start_first_task
   Mô tả: Khởi động task đầu tiên.
   Input: r0 = Stack Pointer ban đầu của task (đang trỏ vào đáy Fake Context)
   ======================================== */
.type start_first_task, %function
start_first_task:
    /* TỐI ƯU HÓA: Thay vì LDMIA để pop rác, ta cộng thẳng địa chỉ */
    /* Stack Frame giả lập có R4-R11 (8 thanh ghi * 4 byte = 32 bytes) nằm đầu */
    
    ADD     r0, r0, #32             /* Nhảy qua vùng R4-R11 giả, trỏ đến R0 */
    MSR     psp, r0                 /* Cài đặt PSP */

    /* Cấu hình CONTROL Register */
    MOVS    r0, #2                  /* Bit 1=1 (Switch to PSP), Bit 0=0 (Privileged) */
    MSR     CONTROL, r0
    
    ISB                             /* Flush pipeline để áp dụng stack mới ngay lập tức */

    /* Nhảy vào Task */
    LDR     lr, =0xFFFFFFFD         /* EXC_RETURN: Thread Mode, PSP */
    BX      lr

    