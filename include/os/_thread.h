/*****************************************************************************
    Author: Yzcc
    Date:   2023.10.12
    Description:    for thread operations
******************************************************************************/
#ifndef INCLUDE_THREAD_H_
#define INCLUDE_THREAD_H_

#include <type.h>
#include <os/sched.h>

pid_t thread_create(uint64_t entry_addr, uint64_t arg);
void thread_yield();

#endif