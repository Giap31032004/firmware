#include "kernel.h"



const char* process_state_str(process_state_t state) {
    switch (state) {
        case PROC_UNUSED:         return "UNUSED";
        case PROC_NEW:            return "NEW";
        case PROC_READY:          return "READY";
        case PROC_RUNNING:        return "RUNNING";
        case PROC_WAITING_TIME:   return "WAIT_TIME";   
        case PROC_WAITING_OBJECT: return "WAIT_OBJ";    
        case PROC_WAITING_IO:     return "WAIT_IO";
        case PROC_SUSPENDED:      return "SUSPENDED";
        case PROC_TERMINATED:     return "DEAD";
        default:                  return "UNKNOWN";
    }
}


/* Hàm so sánh chuỗi (Global) */
int my_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/* Hàm so sánh n ký tự đầu (Global) */
int my_strncmp(const char *s1, const char *s2, int n) {
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}