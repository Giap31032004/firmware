#ifndef MYOS_KERNEL_LIST_MACROS_H
#define MYOS_KERNEL_LIST_MACROS_H

#include "list.h"

typedef list_t ready_list_t;
typedef list_t blocked_list_t;
typedef list_t timer_list_t;
typedef list_t wait_list_t;

#define list_push_ready(list, task)      list_push_back((list), &(task)->node)
#define list_remove_ready(list, task)    list_remove((list), &(task)->node)
#define list_push_wait(list, task)       list_push_back((list), &(task)->node)
#define list_remove_wait(list, task)     list_remove((list), &(task)->node)

#endif /* MYOS_KERNEL_LIST_MACROS_H */
