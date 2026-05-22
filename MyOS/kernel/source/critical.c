#include "critical.h"
#include <stdint.h>
#include "port.h"

/* =========================================================
 * ENTER CRITICAL SECTION
 * ========================================================= */

uint32_t os_enter_critical(void)
{
    uint32_t saved_state;

    saved_state = port_get_irq_state();

    port_disable_irq();

    return saved_state;
}

/* =========================================================
 * EXIT CRITICAL SECTION
 * ========================================================= */

void os_exit_critical(uint32_t saved_state)
{
    port_set_irq_state(saved_state);
}