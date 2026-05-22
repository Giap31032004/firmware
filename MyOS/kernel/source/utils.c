#include "task.h"

const char *task_state_str(task_state_t state)
{
    switch (state) {
    case TASK_UNUSED:
        return "UNUSED";
    case TASK_NEW:
        return "NEW";
    case TASK_READY:
        return "READY";
    case TASK_RUNNING:
        return "RUNNING";
    case TASK_BLOCKED:
        return "BLOCKED";
    case TASK_WAITING_TIME:
        return "WAIT_TIME";
    case TASK_WAITING_OBJECT:
        return "WAIT_OBJ";
    case TASK_WAITING_IO:
        return "WAIT_IO";
    case TASK_SUSPENDED:
        return "SUSPENDED";
    case TASK_TERMINATED:
        return "DEAD";
    default:
        return "UNKNOWN";
    }
}

int my_strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int my_strncmp(const char *s1, const char *s2, int n)
{
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }

    if (n == 0) {
        return 0;
    }

    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}
