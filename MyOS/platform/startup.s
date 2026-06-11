.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb

/* ========================================
   PHẦN 1: KHAI BÁO GLOBAL & EXTERN
   ======================================== */
.global _isr_vector
.global Reset_Handler

.extern _estack

/* Kernel data */
.extern _sidata       /* Load address of .kernel_data in Flash  */
.extern _sdata        /* Start of .kernel_data in KERNEL_RAM    */
.extern _edata        /* End   of .kernel_data in KERNEL_RAM    */

/* Kernel BSS */
.extern _sbss
.extern _ebss

/* User data */
.extern _siuserdata   /* Load address of .user_data in Flash    */
.extern _suserdata    /* Start of .user_data in USER_RAM        */
.extern _euserdata    /* End   of .user_data in USER_RAM        */

/* User BSS */
.extern _suserbss
.extern _euserbss

/* CCM RAM (initialized — stored in Flash, copied at boot) */
.extern _siccmram     /* Load address of .ccmram in Flash       */
.extern _sccmram      /* Start of .ccmram in CCMRAM             */
.extern _eccmram      /* End   of .ccmram in CCMRAM             */

.extern __init_array_start
.extern __init_array_end

.extern SystemInit
.extern main

/* ========================================
   PHẦN 2: VECTOR TABLE
   ======================================== */
.section .isr_vector, "a", %progbits
.align 2
.type _isr_vector, %object

_isr_vector:
/* ===============================
 * EXCEPTIONS CỦA LÕI CORTEX-M4 (0–15)
 * =============================== */
.word _estack                        /* 0  0x00: Initial MSP          */
.word Reset_Handler                  /* 1  0x04: Reset                */
.word NMI_Handler                    /* 2  0x08: NMI                  */
.word HardFault_Handler              /* 3  0x0C: HardFault            */
.word MemManage_Handler              /* 4  0x10: MemManage            */
.word BusFault_Handler               /* 5  0x14: BusFault             */
.word UsageFault_Handler             /* 6  0x18: UsageFault           */
.word 0                              /* 7  0x1C: Reserved             */
.word 0                              /* 8  0x20: Reserved             */
.word 0                              /* 9  0x24: Reserved             */
.word 0                              /* 10 0x28: Reserved             */
.word SVC_Handler                    /* 11 0x2C: SVCall               */
.word DebugMon_Handler               /* 12 0x30: Debug Monitor        */
.word 0                              /* 13 0x34: Reserved             */
.word PendSV_Handler                 /* 14 0x38: PendSV               */
.word SysTick_Handler                /* 15 0x3C: SysTick              */
/* ===============================
 * EXTERNAL INTERRUPTS (IRQ0–IRQ81)
 * =============================== */
.word WWDG_Handler                   /* IRQ0:  Window WatchDog              */
.word PVD_Handler                    /* IRQ1:  PVD through EXTI Line        */
.word TAMP_STAMP_Handler             /* IRQ2:  Tamper / TimeStamp via EXTI  */
.word RTC_WKUP_Handler               /* IRQ3:  RTC Wakeup via EXTI          */
.word FLASH_Handler                  /* IRQ4:  FLASH                        */
.word RCC_Handler                    /* IRQ5:  RCC                          */
.word EXTI0_Handler                  /* IRQ6:  EXTI Line0                   */
.word EXTI1_Handler                  /* IRQ7:  EXTI Line1                   */
.word EXTI2_Handler                  /* IRQ8:  EXTI Line2                   */
.word EXTI3_Handler                  /* IRQ9:  EXTI Line3                   */
.word EXTI4_Handler                  /* IRQ10: EXTI Line4                   */
.word DMA1_Stream0_Handler           /* IRQ11: DMA1 Stream 0                */
.word DMA1_Stream1_Handler           /* IRQ12: DMA1 Stream 1                */
.word DMA1_Stream2_Handler           /* IRQ13: DMA1 Stream 2                */
.word DMA1_Stream3_Handler           /* IRQ14: DMA1 Stream 3                */
.word DMA1_Stream4_Handler           /* IRQ15: DMA1 Stream 4                */
.word DMA1_Stream5_Handler           /* IRQ16: DMA1 Stream 5                */
.word DMA1_Stream6_Handler           /* IRQ17: DMA1 Stream 6                */
.word ADC_Handler                    /* IRQ18: ADC1/2/3                     */
.word CAN1_TX_Handler                /* IRQ19: CAN1 TX                      */
.word CAN1_RX0_Handler               /* IRQ20: CAN1 RX0                     */
.word CAN1_RX1_Handler               /* IRQ21: CAN1 RX1                     */
.word CAN1_SCE_Handler               /* IRQ22: CAN1 SCE                     */
.word EXTI9_5_Handler                /* IRQ23: EXTI Line[9:5]               */
.word TIM1_BRK_TIM9_Handler          /* IRQ24: TIM1 Break + TIM9            */
.word TIM1_UP_TIM10_Handler          /* IRQ25: TIM1 Update + TIM10          */
.word TIM1_TRG_COM_TIM11_Handler     /* IRQ26: TIM1 Trig/Com + TIM11        */
.word TIM1_CC_Handler                /* IRQ27: TIM1 Capture Compare         */
.word TIM2_Handler                   /* IRQ28: TIM2                         */
.word TIM3_Handler                   /* IRQ29: TIM3                         */
.word TIM4_Handler                   /* IRQ30: TIM4                         */
.word I2C1_EV_Handler                /* IRQ31: I2C1 Event                   */
.word I2C1_ER_Handler                /* IRQ32: I2C1 Error                   */
.word I2C2_EV_Handler                /* IRQ33: I2C2 Event                   */
.word I2C2_ER_Handler                /* IRQ34: I2C2 Error                   */
.word SPI1_Handler                   /* IRQ35: SPI1                         */
.word SPI2_Handler                   /* IRQ36: SPI2                         */
.word USART1_Handler                 /* IRQ37: USART1                       */
.word USART2_Handler                 /* IRQ38: USART2                       */
.word USART3_Handler                 /* IRQ39: USART3                       */
.word EXTI15_10_Handler              /* IRQ40: EXTI Line[15:10]             */
.word RTC_Alarm_Handler              /* IRQ41: RTC Alarm via EXTI           */
.word OTG_FS_WKUP_Handler            /* IRQ42: USB OTG FS Wakeup via EXTI   */
.word TIM8_BRK_TIM12_Handler         /* IRQ43: TIM8 Break + TIM12           */
.word TIM8_UP_TIM13_Handler          /* IRQ44: TIM8 Update + TIM13          */
.word TIM8_TRG_COM_TIM14_Handler     /* IRQ45: TIM8 Trig/Com + TIM14        */
.word TIM8_CC_Handler                /* IRQ46: TIM8 Capture Compare         */
.word DMA1_Stream7_Handler           /* IRQ47: DMA1 Stream7                 */
.word FSMC_Handler                   /* IRQ48: FSMC                         */
.word SDIO_Handler                   /* IRQ49: SDIO                         */
.word TIM5_Handler                   /* IRQ50: TIM5                         */
.word SPI3_Handler                   /* IRQ51: SPI3                         */
.word UART4_Handler                  /* IRQ52: UART4                        */
.word UART5_Handler                  /* IRQ53: UART5                        */
.word TIM6_DAC_Handler               /* IRQ54: TIM6 + DAC underrun          */
.word TIM7_Handler                   /* IRQ55: TIM7                         */
.word DMA2_Stream0_Handler           /* IRQ56: DMA2 Stream 0                */
.word DMA2_Stream1_Handler           /* IRQ57: DMA2 Stream 1                */
.word DMA2_Stream2_Handler           /* IRQ58: DMA2 Stream 2                */
.word DMA2_Stream3_Handler           /* IRQ59: DMA2 Stream 3                */
.word DMA2_Stream4_Handler           /* IRQ60: DMA2 Stream 4                */
.word ETH_Handler                    /* IRQ61: Ethernet                     */
.word ETH_WKUP_Handler               /* IRQ62: Ethernet Wakeup via EXTI     */
.word CAN2_TX_Handler                /* IRQ63: CAN2 TX                      */
.word CAN2_RX0_Handler               /* IRQ64: CAN2 RX0                     */
.word CAN2_RX1_Handler               /* IRQ65: CAN2 RX1                     */
.word CAN2_SCE_Handler               /* IRQ66: CAN2 SCE                     */
.word OTG_FS_Handler                 /* IRQ67: USB OTG FS                   */
.word DMA2_Stream5_Handler           /* IRQ68: DMA2 Stream 5                */
.word DMA2_Stream6_Handler           /* IRQ69: DMA2 Stream 6                */
.word DMA2_Stream7_Handler           /* IRQ70: DMA2 Stream 7                */
.word USART6_Handler                 /* IRQ71: USART6                       */
.word I2C3_EV_Handler                /* IRQ72: I2C3 Event                   */
.word I2C3_ER_Handler                /* IRQ73: I2C3 Error                   */
.word OTG_HS_EP1_OUT_Handler         /* IRQ74: USB OTG HS EP1 Out           */
.word OTG_HS_EP1_IN_Handler          /* IRQ75: USB OTG HS EP1 In            */
.word OTG_HS_WKUP_Handler            /* IRQ76: USB OTG HS Wakeup via EXTI   */
.word OTG_HS_Handler                 /* IRQ77: USB OTG HS                   */
.word DCMI_Handler                   /* IRQ78: DCMI                         */
.word CRYP_Handler                   /* IRQ79: CRYP                         */
.word HASH_RNG_Handler               /* IRQ80: Hash + RNG                   */
.word FPU_Handler                    /* IRQ81: FPU                          */

.size _isr_vector, . - _isr_vector

/* ========================================
   PHẦN 3: RESET HANDLER
   ======================================== */
/* Đặt vào .kernel_text để đảm bảo nằm trong KERNEL_FLASH */
.section .kernel_text, "ax", %progbits
.align 2
.weak  Reset_Handler
.type  Reset_Handler, %function

Reset_Handler:
    /* --------------------------------------------------------
       1. SystemInit — cấu hình clock 168 MHz, FPU, Flash wait
          Phải gọi TRƯỚC khi copy data để tăng tốc quá trình boot
       -------------------------------------------------------- */
    bl SystemInit

    /* --------------------------------------------------------
       2. Copy .kernel_data: Flash → KERNEL_RAM
          Symbols: _sidata (src), _sdata/_edata (dst range)
       -------------------------------------------------------- */
    ldr  r0, =_sdata
    ldr  r1, =_edata
    ldr  r2, =_sidata
    b    copy_kdata_check
copy_kdata_loop:
    ldr  r3, [r2], #4
    str  r3, [r0], #4
copy_kdata_check:
    cmp  r0, r1
    bcc  copy_kdata_loop

    /* --------------------------------------------------------
       3. Copy .user_data: Flash → USER_RAM
          Symbols: _siuserdata (src), _suserdata/_euserdata (dst)
       -------------------------------------------------------- */
    ldr  r0, =_suserdata
    ldr  r1, =_euserdata
    ldr  r2, =_siuserdata
    b    copy_udata_check
copy_udata_loop:
    ldr  r3, [r2], #4
    str  r3, [r0], #4
copy_udata_check:
    cmp  r0, r1
    bcc  copy_udata_loop

    /* --------------------------------------------------------
       4. Copy .ccmram: Flash → CCMRAM
          (CCM RAM mất điện khi reset nên phải copy lại từ Flash)
          Symbols: _siccmram (src), _sccmram/_eccmram (dst)
       -------------------------------------------------------- */
    ldr  r0, =_sccmram
    ldr  r1, =_eccmram
    ldr  r2, =_siccmram
    b    copy_ccmram_check
copy_ccmram_loop:
    ldr  r3, [r2], #4
    str  r3, [r0], #4
copy_ccmram_check:
    cmp  r0, r1
    bcc  copy_ccmram_loop

    /* --------------------------------------------------------
       5. Zero-fill .kernel_bss
       -------------------------------------------------------- */
    ldr  r0, =_sbss
    ldr  r1, =_ebss
    movs r2, #0
    b    zero_kbss_check
zero_kbss_loop:
    str  r2, [r0], #4
zero_kbss_check:
    cmp  r0, r1
    bcc  zero_kbss_loop

    /* --------------------------------------------------------
       6. Zero-fill .user_bss
       -------------------------------------------------------- */
    ldr  r0, =_suserbss
    ldr  r1, =_euserbss
    movs r2, #0
    b    zero_ubss_check
zero_ubss_loop:
    str  r2, [r0], #4
zero_ubss_check:
    cmp  r0, r1
    bcc  zero_ubss_loop

    /* --------------------------------------------------------
       7. C++ / libc constructors (.init_array)
       -------------------------------------------------------- */
    ldr  r0, =__init_array_start
    ldr  r1, =__init_array_end
    b    call_init_check
call_init_loop:
    ldr  r2, [r0], #4   /* Đọc con trỏ hàm khởi tạo                  */
    push {r0, r1}        /* Cất r0/r1 — hàm con có thể clobber chúng  */
    blx  r2              /* Gọi hàm khởi tạo                           */
    pop  {r0, r1}        /* Khôi phục r0/r1                            */
call_init_check:
    cmp  r0, r1
    bcc  call_init_loop

    /* --------------------------------------------------------
       8. Vào main — nếu main trả về, dừng tại đây
       -------------------------------------------------------- */
    bl   main
halt_loop:
    b    halt_loop

.size Reset_Handler, . - Reset_Handler

/* ========================================
   PHẦN 4: WEAK ALIAS DEFINITIONS (STM32F407)
   ======================================== */
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

/* External Interrupts IRQ0–IRQ81 */
    def_irq_handler WWDG_Handler
    def_irq_handler PVD_Handler
    def_irq_handler TAMP_STAMP_Handler
    def_irq_handler RTC_WKUP_Handler
    def_irq_handler FLASH_Handler
    def_irq_handler RCC_Handler
    def_irq_handler EXTI0_Handler
    def_irq_handler EXTI1_Handler
    def_irq_handler EXTI2_Handler
    def_irq_handler EXTI3_Handler
    def_irq_handler EXTI4_Handler
    def_irq_handler DMA1_Stream0_Handler
    def_irq_handler DMA1_Stream1_Handler
    def_irq_handler DMA1_Stream2_Handler
    def_irq_handler DMA1_Stream3_Handler
    def_irq_handler DMA1_Stream4_Handler
    def_irq_handler DMA1_Stream5_Handler
    def_irq_handler DMA1_Stream6_Handler
    def_irq_handler ADC_Handler
    def_irq_handler CAN1_TX_Handler
    def_irq_handler CAN1_RX0_Handler
    def_irq_handler CAN1_RX1_Handler
    def_irq_handler CAN1_SCE_Handler
    def_irq_handler EXTI9_5_Handler
    def_irq_handler TIM1_BRK_TIM9_Handler
    def_irq_handler TIM1_UP_TIM10_Handler
    def_irq_handler TIM1_TRG_COM_TIM11_Handler
    def_irq_handler TIM1_CC_Handler
    def_irq_handler TIM2_Handler
    def_irq_handler TIM3_Handler
    def_irq_handler TIM4_Handler
    def_irq_handler I2C1_EV_Handler
    def_irq_handler I2C1_ER_Handler
    def_irq_handler I2C2_EV_Handler
    def_irq_handler I2C2_ER_Handler
    def_irq_handler SPI1_Handler
    def_irq_handler SPI2_Handler
    def_irq_handler USART1_Handler
    def_irq_handler USART2_Handler
    def_irq_handler USART3_Handler
    def_irq_handler EXTI15_10_Handler
    def_irq_handler RTC_Alarm_Handler
    def_irq_handler OTG_FS_WKUP_Handler
    def_irq_handler TIM8_BRK_TIM12_Handler
    def_irq_handler TIM8_UP_TIM13_Handler
    def_irq_handler TIM8_TRG_COM_TIM14_Handler
    def_irq_handler TIM8_CC_Handler
    def_irq_handler DMA1_Stream7_Handler
    def_irq_handler FSMC_Handler
    def_irq_handler SDIO_Handler
    def_irq_handler TIM5_Handler
    def_irq_handler SPI3_Handler
    def_irq_handler UART4_Handler
    def_irq_handler UART5_Handler
    def_irq_handler TIM6_DAC_Handler
    def_irq_handler TIM7_Handler
    def_irq_handler DMA2_Stream0_Handler
    def_irq_handler DMA2_Stream1_Handler
    def_irq_handler DMA2_Stream2_Handler
    def_irq_handler DMA2_Stream3_Handler
    def_irq_handler DMA2_Stream4_Handler
    def_irq_handler ETH_Handler
    def_irq_handler ETH_WKUP_Handler
    def_irq_handler CAN2_TX_Handler
    def_irq_handler CAN2_RX0_Handler
    def_irq_handler CAN2_RX1_Handler
    def_irq_handler CAN2_SCE_Handler
    def_irq_handler OTG_FS_Handler
    def_irq_handler DMA2_Stream5_Handler
    def_irq_handler DMA2_Stream6_Handler
    def_irq_handler DMA2_Stream7_Handler
    def_irq_handler USART6_Handler
    def_irq_handler I2C3_EV_Handler
    def_irq_handler I2C3_ER_Handler
    def_irq_handler OTG_HS_EP1_OUT_Handler
    def_irq_handler OTG_HS_EP1_IN_Handler
    def_irq_handler OTG_HS_WKUP_Handler
    def_irq_handler OTG_HS_Handler
    def_irq_handler DCMI_Handler
    def_irq_handler CRYP_Handler
    def_irq_handler HASH_RNG_Handler
    def_irq_handler FPU_Handler

/* ========================================
   PHẦN 5: DEFAULT HANDLER
   ======================================== */
.section .kernel_text.Default_Handler, "ax", %progbits
.align 2
.global Default_Handler
.type   Default_Handler, %function

Default_Handler:
    b .     /* Spin tại chỗ — dễ nhận ra trong debugger */

.size Default_Handler, . - Default_Handler
