/*******************************************************
    Author:         Yzcc
    Date:           2023.11.24
    Description:    for thread operation
                    (abandon '_thread.h')
********************************************************/
#ifndef INCLUDE_PTHREAD_H_
#define INCLUDE_PTHREAD_H_

#include <type.h>
#include <os/sched.h>

pid_t pthread_create(uint64_t entry_addr, void *arg);
int pthread_join(pid_t thread_id);

#endif