#ifndef CRITICAL_H
#define CRITICAL_H

#include <stdint.h>

/* =========================================================
 * CRITICAL SECTION API
 * ========================================================= */

uint32_t os_enter_critical(void);

void os_exit_critical(uint32_t saved_state);

#endif /* CRITICAL_H */