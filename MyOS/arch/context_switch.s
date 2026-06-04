.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb

.global PendSV_Handler
.global SVC_Handler

.extern current_tcb
.extern next_tcb

.section .text

/* Context switch handler. Runs in Handler mode. */
.type PendSV_Handler, %function
PendSV_Handler:
    MRS     r0, psp
    CBZ     r0, load_next_task

    /* If the outgoing task used FPU, save S16-S31. */
    TST     lr, #0x10
    IT      EQ
    VSTMDBEQ r0!, {s16-s31}

    /* Save software context: R4-R11 and EXC_RETURN in LR. */
    LDR     r1, =current_tcb
    LDR     r1, [r1]
    CBZ     r1, load_next_task

    STMDB   r0!, {r4-r11, lr}
    STR     r0, [r1]

load_next_task:
    LDR     r1, =next_tcb
    LDR     r1, [r1]
    CBZ     r1, pend_exit

    LDR     r0, [r1]

    LDR     r2, =current_tcb
    STR     r1, [r2]

    /* Restore software context: R4-R11 and EXC_RETURN in LR. */
    LDMIA   r0!, {r4-r11, lr}

    /* If the incoming task used FPU, restore S16-S31. */
    TST     lr, #0x10
    IT      EQ
    VLDMIAEQ r0!, {s16-s31}

    MSR     psp, r0

    DSB
    ISB

pend_exit:
    BX      lr

/* Start the first task from a real exception context.
 * port_start_scheduler() enters here with SVC #0.
 */
.type SVC_Handler, %function
SVC_Handler:
    LDR     r0, =current_tcb
    LDR     r0, [r0]
    CBZ     r0, svc_exit

    LDR     r0, [r0]                /* r0 = current_tcb->stack_ptr */

    /* task_create_at() builds the same software frame PendSV restores. */
    LDMIA   r0!, {r4-r11, lr}
    MSR     psp, r0                 /* PSP points to the hardware frame. */

    MOVS    r0, #2                  /* Thread mode uses PSP, privileged. */
    MSR     CONTROL, r0

    DSB
    ISB

    BX      lr                      /* Exception return into first task. */

svc_exit:
    BX      lr
