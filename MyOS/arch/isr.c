/* =========================================================
 * SysTick Interrupt Service Routine (ISR)
 * Bridge: Hardware → OS kernel
 * ========================================================= */

#include "tick.h"

#ifdef __cplusplus
extern "C" {
#endif

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
