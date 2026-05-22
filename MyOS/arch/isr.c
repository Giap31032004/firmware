/* =========================================================
 * SysTick Interrupt Service Routine (ISR)
 * Bridge: Hardware → OS kernel
 * ========================================================= */

#ifdef __cplusplus
extern "C" {
#endif

/* Kernel tick function (implemented in OS kernel layer) */
extern void kernel_tick(void);

/* =========================================================
 * SysTick Handler
 * Called automatically by CPU when SysTick interrupt fires
 * ========================================================= */
void SysTick_Handler(void)
{
    kernel_tick();
}

#ifdef __cplusplus
}
#endif