#ifndef MYOS_KERNEL_BANKER_H
#define MYOS_KERNEL_BANKER_H

#include <stdint.h>

/* ================= RESOURCE MODEL ================= */
#define NUM_RESOURCES 3

typedef enum {
    RES_UART = 0,
    RES_I2C,
    RES_DMA_CH
} resource_type_t;

/* ================= DATA OWNED BY BANKER ================= */
typedef struct resource_info {
    int held[NUM_RESOURCES];
    int max[NUM_RESOURCES];
} resource_info_t;

/* ================= SYSTEM STATE ================= */
extern int system_available[NUM_RESOURCES];

/* ================= BANKER API ================= */
/* Banker operates on TASK, but does NOT own it */
void banker_init(void);
int request_resources(int request[]);
void release_resources(int release[]);

#endif /* MYOS_KERNEL_BANKER_H */
