#ifndef FAULT_H
#define FAULT_H

#include <stdint.h>

typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
} Fault_StackFrame_t;

typedef struct {
    Fault_StackFrame_t *stack_frame;
    uint32_t exc_return;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t mmar;
    uint32_t bfar;
} Fault_Context_t;

void fault_subsystem_init(void);

#endif // FAULT_H