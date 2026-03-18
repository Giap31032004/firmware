.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb

.global PendSV_Handler
.global start_first_task

/* External variables from C */
.extern current_pcb
.extern next_pcb

.section .text

/* ========================================
   HÀM: PendSV_Handler (Dành cho Cortex-M4F)
   Mô tả: Thực hiện Context Switch có hỗ trợ FPU
   ======================================== */
.type PendSV_Handler, %function
PendSV_Handler:
    /* 1. Lấy PSP hiện tại */
    MRS     r0, psp
    CBZ     r0, load_next_task      /* Nếu PSP=0 (chưa chạy task nào), nhảy đến load luôn */

    /* 2. Kiểm tra xem Task cũ có dùng FPU không (Dựa vào Bit 4 của LR) */
    TST     lr, #0x10               /* Test bit 4 của EXC_RETURN */
    IT      EQ                      /* Khối điều kiện If-Then (Nếu Bit 4 == 0) */
    VSTMDBEQ r0!, {s16-s31}         /* Push thêm S16-S31 vào stack (S0-S15 phần cứng đã tự push) */

    /* 3. Lưu Context cũ: Push R4-R11 VÀ thanh ghi LR (rất quan trọng) */
    LDR     r1, =current_pcb
    LDR     r1, [r1]
    CBZ     r1, load_next_task      /* Safety check */

    STMDB   r0!, {r4-r11, lr}       /* Push R4-R11 và LR (LR đang chứa EXC_RETURN của task này) */
    STR     r0, [r1]                /* Lưu SP mới vào current_pcb->stack_ptr */

load_next_task:
    /* 4. Lấy Task tiếp theo */
    LDR     r1, =next_pcb
    LDR     r1, [r1]
    CBZ     r1, pend_exit           /* Safety check */

    LDR     r0, [r1]                /* r0 = next_pcb->stack_ptr */
    
    /* 5. Cập nhật current_pcb = next_pcb */
    LDR     r2, =current_pcb
    STR     r1, [r2]

    /* 6. Khôi phục Context mới: Pop R4-R11 VÀ thanh ghi LR */
    LDMIA   r0!, {r4-r11, lr}       /* Lúc này LR chứa đúng mã EXC_RETURN của task chuẩn bị chạy */

    /* 7. Kiểm tra xem Task mới có cần FPU không (Dựa vào LR vừa Pop ra) */
    TST     lr, #0x10               
    IT      EQ                      
    VLDMIAEQ r0!, {s16-s31}         /* Nếu có, pop S16-S31 trả lại cho bộ FPU */

    MSR     psp, r0                 /* Cập nhật thanh ghi PSP */
    
    /* 8. Barriers (Quan trọng) */
    DSB                             /* Data Synchronization Barrier */
    ISB                             /* Instruction Synchronization Barrier */

pend_exit:
    /* 9. Thoát ngắt */
    /* Không cần ORR rác nữa vì ta lấy trực tiếp lệnh BX LR, 
       bản thân LR đã chứa EXC_RETURN chính xác của Task đó rồi! */
    BX      lr

/* ========================================
   HÀM: start_first_task
   ======================================== */
.type start_first_task, %function
start_first_task:
    /* TỐI ƯU HÓA CHO M4F */
    /* Vùng giả lập Context giờ có 9 thanh ghi: r4-r11 (8) + lr (1) */
    /* 9 * 4 bytes = 36 bytes. Ta phải cộng 36 thay vì 32 để nhảy qua nó */
    
    ADD     r0, r0, #36             /* Nhảy qua vùng R4-R11 và LR giả */
    MSR     psp, r0                 /* Cài đặt PSP */

    /* Cấu hình CONTROL Register */
    MOVS    r0, #2                  /* Bit 1=1 (Switch to PSP), Bit 0=0 (Privileged) */
    MSR     CONTROL, r0
    
    ISB                             /* Flush pipeline */

    /* Nhảy vào Task đầu tiên */
    LDR     lr, =0xFFFFFFFD         /* EXC_RETURN mặc định: Thread Mode, dùng PSP, không FPU */
    BX      lr
    