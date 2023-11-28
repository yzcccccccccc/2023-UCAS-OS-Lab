#include <pthread.h>
#include <security_page.h>
#include <unistd.h>

/* TODO:[P4-task4] pthread_create/wait */
void pthread_create(pthread_t *thread,
                   void (*start_routine)(void*),
                   void *arg)
{
    /* TODO: [p4-task4] implement pthread_create */
    // Step1: check handle and init
    init_secPage_mlock();
    // Step2: syscall
    *thread = sys_thread_create((uint64_t)start_routine, arg);
}

int pthread_join(pthread_t thread)
{
    /* TODO: [p4-task4] implement pthread_join */
    return sys_thread_join(thread);
}
