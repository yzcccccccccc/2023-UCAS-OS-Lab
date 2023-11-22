#include <atomic.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/lock.h>
#include <os/kernel.h>

spin_lock_t kernel_lock;
int kernel_cpu_id = -1;

void smp_init()
{
    /* TODO: P3-TASK3 multicore*/
    spin_lock_init(&kernel_lock);
}

void wakeup_other_hart()
{
    /* TODO: P3-TASK3 multicore*/
    const unsigned long mask = 0x2;
    send_ipi(&mask);
}

void lock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
    spin_lock_acquire(&kernel_lock);
    kernel_cpu_id = get_current_cpu_id();
}

void unlock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
    kernel_cpu_id = -1;
    spin_lock_release(&kernel_lock);
}
