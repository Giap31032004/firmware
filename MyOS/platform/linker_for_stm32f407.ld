/* STM32F407 MPU-AWARE MEMORY MAP */
ENTRY(Reset_Handler)

/* ================= MEMORY SPLIT ================= */
MEMORY
{
  /* KERNEL FLASH (OS code) */
  KERNEL_FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K

  /* USER FLASH (apps/tasks) */
  USER_FLASH   (rx)  : ORIGIN = 0x08010000, LENGTH = 448K

  /* KERNEL RAM (RTOS core data) */
  KERNEL_RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 32K

  /* USER RAM (explicit .user_data/.user_bss sections) */
  USER_RAM     (xrw) : ORIGIN = 0x20008000, LENGTH = 80K

  /* CCM RAM (fast kernel stack / critical buffers) */
  CCMRAM       (xrw) : ORIGIN = 0x10000000, LENGTH = 64K
}

/* Kernel stack at top of kernel RAM */
PROVIDE(_estack = ORIGIN(KERNEL_RAM) + LENGTH(KERNEL_RAM));

/* Memory-region boundaries exported for MPU setup and diagnostics */
PROVIDE(_kernel_flash_start = ORIGIN(KERNEL_FLASH));
PROVIDE(_kernel_flash_end   = ORIGIN(KERNEL_FLASH) + LENGTH(KERNEL_FLASH));
PROVIDE(_user_flash_start   = ORIGIN(USER_FLASH));
PROVIDE(_user_flash_end     = ORIGIN(USER_FLASH) + LENGTH(USER_FLASH));
PROVIDE(_kernel_ram_start   = ORIGIN(KERNEL_RAM));
PROVIDE(_kernel_ram_end     = ORIGIN(KERNEL_RAM) + LENGTH(KERNEL_RAM));
PROVIDE(_user_ram_start     = ORIGIN(USER_RAM));
PROVIDE(_user_ram_end       = ORIGIN(USER_RAM) + LENGTH(USER_RAM));
PROVIDE(_ccmram_start       = ORIGIN(CCMRAM));
PROVIDE(_ccmram_end         = ORIGIN(CCMRAM) + LENGTH(CCMRAM));

SECTIONS
{
  /* ================= VECTOR TABLE ================= */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >KERNEL_FLASH

  /* ================= KERNEL CODE ================= */
  /* Only pulls sections explicitly marked .kernel_text/.kernel_rodata */
  .kernel_text :
  {
    . = ALIGN(4);
    *(.kernel_text .kernel_text*)
    *(.kernel_rodata .kernel_rodata*)
    . = ALIGN(4);
  } >KERNEL_FLASH

  /* ================= ARM EXCEPTIONS ================= */
  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >KERNEL_FLASH

  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >KERNEL_FLASH

  /* ================= INIT / FINI ARRAYS ================= */
  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } >KERNEL_FLASH

  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } >KERNEL_FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } >KERNEL_FLASH

  /* ================= USER CODE ================= */
  /* General .text/.rodata land here; explicitly marked .user_text also here */
  .user_text :
  {
    . = ALIGN(4);
    *(.user_text .user_text*)
    *(.text .text*)
    *(.rodata .rodata*)
    . = ALIGN(4);
  } >USER_FLASH

  /* ---- Load-address symbols (must precede AT> sections) ---- */
  _sidata     = LOADADDR(.kernel_data);
  _siuserdata = LOADADDR(.user_data);
  _siccmram   = LOADADDR(.ccmram);

  /* ================= KERNEL DATA ================= */
  .kernel_data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.kernel_data .kernel_data*)
    *(.data .data*)       /* general initialized data → kernel RAM */
    . = ALIGN(4);
    _edata = .;
  } >KERNEL_RAM AT> KERNEL_FLASH

  /* ================= USER DATA ================= */
  .user_data :
  {
    . = ALIGN(4);
    _suserdata = .;
    *(.user_data .user_data*)
    . = ALIGN(4);
    _euserdata = .;
  } >USER_RAM AT> USER_FLASH

  /* ================= CCM RAM ================= */
  /* Initialized CCM data is stored in KERNEL_FLASH and copied at startup */
  .ccmram :
  {
    . = ALIGN(4);
    _sccmram = .;
    *(.ccmram .ccmram*)
    . = ALIGN(4);
    _eccmram = .;
  } >CCMRAM AT> KERNEL_FLASH

  /* ================= KERNEL BSS ================= */
  .kernel_bss :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.kernel_bss .kernel_bss*)
    *(.bss .bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } >KERNEL_RAM

  /* ================= USER BSS ================= */
  .user_bss :
  {
    . = ALIGN(4);
    _suserbss = .;
    *(.user_bss .user_bss*)
    . = ALIGN(4);
    _euserbss = .;
  } >USER_RAM

  /* Kernel heap allocator starts at _end and grows toward _estack - 4K. */
  PROVIDE(_end = _ebss);

  /* ================= OVERFLOW GUARDS ================= */
  ASSERT(ORIGIN(KERNEL_RAM) + LENGTH(KERNEL_RAM) >= _estack,
         "ERROR: KERNEL_RAM overflow — reduce data/bss or increase LENGTH")
  ASSERT(ORIGIN(USER_RAM)   + LENGTH(USER_RAM)   >= _euserbss,
         "ERROR: USER_RAM overflow — reduce user data/bss or increase LENGTH")
  ASSERT(ORIGIN(CCMRAM)     + LENGTH(CCMRAM)     >= _eccmram,
         "ERROR: CCMRAM overflow")
  ASSERT(ORIGIN(KERNEL_FLASH) + LENGTH(KERNEL_FLASH) >= LOADADDR(.user_text),
         "ERROR: KERNEL_FLASH overflow — kernel code/data too large")
}
