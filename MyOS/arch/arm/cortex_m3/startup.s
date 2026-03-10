.syntax unified
.cpu cortex-m3
.thumb

/* ========================================
   PHẦN 1: KHAI BÁO GLOBAL & EXTERN
   ======================================== */
.global vector_table 
.global Reset_Handler 

.extern _estack 
.extern _sidata   
.extern _sdata    
.extern _edata    
.extern _sbss     
.extern _ebss     

.extern SystemInit 
.extern main  

/* ========================================
   PHẦN 2: VECTOR TABLE
   ======================================== */
.section .isr_vector, "a", %progbits
.align 2
.type vector_table, %object

vector_table:
/* ===============================
 * EXCEPTIONS CỦA LÕI CORTEX-M3 (0–15)
 * =============================== */
.word _estack                  /* 0  0x00: Initial MSP */
.word Reset_Handler            /* 1  0x04: Reset */
.word NMI_Handler              /* 2  0x08: NMI */
.word HardFault_Handler        /* 3  0x0C: HardFault */
.word MemManage_Handler        /* 4  0x10: MemManage */
.word BusFault_Handler         /* 5  0x14: BusFault */
.word UsageFault_Handler       /* 6  0x18: UsageFault */
.word 0                        /* 7  0x1C: Reserved */
.word 0                        /* 8  0x20: Reserved */
.word 0                        /* 9  0x24: Reserved */
.word 0                        /* 10 0x28: Reserved */
.word SVC_Handler              /* 11 0x2C: SVCall */
.word DebugMon_Handler         /* 12 0x30: Debug Monitor */
.word 0                        /* 13 0x34: Reserved */
.word PendSV_Handler           /* 14 0x38: PendSV */
.word SysTick_Handler          /* 15 0x3C: SysTick */

/* ===============================
 * EXTERNAL INTERRUPTS CỦA LM3S6965 (IRQ0–IRQ43)
 * Phải khớp tuyệt đối với Table 2-9
 * =============================== */
.word GPIOA_Handler            /* 16  IRQ0:  GPIO Port A */ 
.word GPIOB_Handler            /* 17  IRQ1:  GPIO Port B */
.word GPIOC_Handler            /* 18  IRQ2:  GPIO Port C */
.word GPIOD_Handler            /* 19  IRQ3:  GPIO Port D */
.word GPIOE_Handler            /* 20  IRQ4:  GPIO Port E */
.word UART0_Handler            /* 21  IRQ5:  UART0 */
.word UART1_Handler            /* 22  IRQ6:  UART1 */
.word SSI0_Handler             /* 23  IRQ7:  SSI0 */
.word I2C0_Handler             /* 24  IRQ8:  I2C0 */
.word PWM_FAULT_Handler        /* 25  IRQ9:  PWM Fault */
.word PWM_GEN0_Handler         /* 26  IRQ10: PWM Generator 0 */
.word PWM_GEN1_Handler         /* 27  IRQ11: PWM Generator 1 */
.word PWM_GEN2_Handler         /* 28  IRQ12: PWM Generator 2 */
.word QEI0_Handler             /* 29  IRQ13: QEI0 */
.word ADC0_SEQ0_Handler        /* 30  IRQ14: ADC0 Sequence 0 */
.word ADC0_SEQ1_Handler        /* 31  IRQ15: ADC0 Sequence 1 */
.word ADC0_SEQ2_Handler        /* 32  IRQ16: ADC0 Sequence 2 */
.word ADC0_SEQ3_Handler        /* 33  IRQ17: ADC0 Sequence 3 */
.word WATCHDOG_Handler         /* 34  IRQ18: Watchdog Timer 0 */
.word TIMER0A_Handler          /* 35  IRQ19: Timer 0A */
.word TIMER0B_Handler          /* 36  IRQ20: Timer 0B */
.word TIMER1A_Handler          /* 37  IRQ21: Timer 1A */
.word TIMER1B_Handler          /* 38  IRQ22: Timer 1B */
.word TIMER2A_Handler          /* 39  IRQ23: Timer 2A */
.word TIMER2B_Handler          /* 40  IRQ24: Timer 2B */
.word COMP0_Handler            /* 41  IRQ25: Analog Comparator 0 */
.word COMP1_Handler            /* 42  IRQ26: Analog Comparator 1 */
.word 0                        /* 43  IRQ27: Reserved */
.word SYSCTL_Handler           /* 44  IRQ28: System Control */
.word FLASH_Handler            /* 45  IRQ29: Flash Memory Control */
.word GPIOF_Handler            /* 46  IRQ30: GPIO Port F */
.word GPIOG_Handler            /* 47  IRQ31: GPIO Port G */
.word 0                        /* 48  IRQ32: Reserved */
.word UART2_Handler            /* 49  IRQ33: UART2 */
.word 0                        /* 50  IRQ34: Reserved */
.word TIMER3A_Handler          /* 51  IRQ35: Timer 3A */
.word TIMER3B_Handler          /* 52  IRQ36: Timer 3B */
.word I2C1_Handler             /* 53  IRQ37: I2C1 */
.word QEI1_Handler             /* 54  IRQ38: QEI1 */
.word 0                        /* 55  IRQ39: Reserved */
.word 0                        /* 56  IRQ40: Reserved */
.word 0                        /* 57  IRQ41: Reserved */
.word ETH_Handler              /* 58  IRQ42: Ethernet Controller */
.word HIBERNATE_Handler        /* 59  IRQ43: Hibernation Module */

.size vector_table, . - vector_table

/* ========================================
   PHẦN 3: RESET HANDLER 
   ======================================== */
.section .text 
.align 2
.weak Reset_Handler
.type Reset_Handler, %function

Reset_Handler:
    /* Cấu hình Xung nhịp */
    bl SystemInit

    /* 1. Copy .data từ Flash sang RAM */
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
    movs r3, #0
    b loop_copy_data

copy_data:
    ldr r4, [r2, r3]
    str r4, [r0, r3]
    adds r3, r3, #4

loop_copy_data:
    adds r4, r0, r3
    cmp r4, r1
    bcc copy_data

    /* 2. Xóa .bss về 0 */
    ldr r2, =_sbss
    ldr r4, =_ebss
    movs r3, #0
    b loop_zero_bss

zero_bss:
    str r3, [r2]
    adds r2, r2, #4

loop_zero_bss:
    cmp r2, r4
    bcc zero_bss

    /* 3. Vào Main */
    bl main
    b .

.size Reset_Handler, . - Reset_Handler

/* Default Handler */
.section .text.Default_Handler
.weak Default_Handler
.type Default_Handler, %function
Default_Handler: 
    b .

/* ===================================================
   PHẦN 4: WEAK ALIAS DEFINITIONS
   =================================================== */
    .macro def_irq_handler handler_name
    .weak \handler_name
    .set  \handler_name, Default_Handler
    .endm

    /* Core Exceptions */
    def_irq_handler NMI_Handler
    def_irq_handler HardFault_Handler
    def_irq_handler MemManage_Handler
    def_irq_handler BusFault_Handler
    def_irq_handler UsageFault_Handler
    def_irq_handler SVC_Handler
    def_irq_handler DebugMon_Handler
    def_irq_handler PendSV_Handler
    def_irq_handler SysTick_Handler

    /* External Interrupts */
    def_irq_handler GPIOA_Handler
    def_irq_handler GPIOB_Handler
    def_irq_handler GPIOC_Handler
    def_irq_handler GPIOD_Handler
    def_irq_handler GPIOE_Handler
    def_irq_handler UART0_Handler
    def_irq_handler UART1_Handler
    def_irq_handler SSI0_Handler
    def_irq_handler I2C0_Handler
    def_irq_handler PWM_FAULT_Handler
    def_irq_handler PWM_GEN0_Handler
    def_irq_handler PWM_GEN1_Handler
    def_irq_handler PWM_GEN2_Handler
    def_irq_handler QEI0_Handler
    def_irq_handler ADC0_SEQ0_Handler
    def_irq_handler ADC0_SEQ1_Handler
    def_irq_handler ADC0_SEQ2_Handler
    def_irq_handler ADC0_SEQ3_Handler
    def_irq_handler WATCHDOG_Handler
    def_irq_handler TIMER0A_Handler
    def_irq_handler TIMER0B_Handler
    def_irq_handler TIMER1A_Handler
    def_irq_handler TIMER1B_Handler
    def_irq_handler TIMER2A_Handler
    def_irq_handler TIMER2B_Handler
    def_irq_handler COMP0_Handler
    def_irq_handler COMP1_Handler
    def_irq_handler SYSCTL_Handler
    def_irq_handler FLASH_Handler
    def_irq_handler GPIOF_Handler
    def_irq_handler GPIOG_Handler
    def_irq_handler UART2_Handler
    def_irq_handler TIMER3A_Handler
    def_irq_handler TIMER3B_Handler
    def_irq_handler I2C1_Handler
    def_irq_handler QEI1_Handler
    def_irq_handler ETH_Handler
    def_irq_handler HIBERNATE_Handler
    