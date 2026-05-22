#ifndef MYOS_KERNEL_UTILS_H
#define MYOS_KERNEL_UTILS_H

#include "task.h"

const char *task_state_str(task_state_t state);
int my_strcmp(const char *s1, const char *s2);
int my_strncmp(const char *s1, const char *s2, int n);

#endif /* MYOS_KERNEL_UTILS_H */
