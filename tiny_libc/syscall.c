#include <syscall.h>
#include <stdint.h>
#include <kernel.h>
#include <unistd.h>

static const long IGNORE = 0L;

static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,
                           long arg3, long arg4)
{
    /* TODO: [p2-task3] implement invoke_syscall via inline assembly */
    long rtval;
    asm volatile("mv a7, %1\n\t"
                "mv a0, %2\n\t"
                "mv a1, %3\n\t"
                "mv a2, %4\n\t"
                "mv a3, %5\n\t"
                "mv a4, %6\n\t"
                "ecall\n\t"
                "mv %0, a0"
                :"=r"(rtval)
                :"r"(sysno), "r"(arg0), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4)
                :"a0", "a1", "a2", "a3", "a4", "a7"
            );

    return rtval;
}

void sys_yield(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_yield */
    invoke_syscall(SYSCALL_YIELD, IGNORE, 0, 0, 0, 0);
}

void sys_move_cursor(int x, int y)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_move_cursor */
    invoke_syscall(SYSCALL_CURSOR, (long)x, (long)y, 0, 0, 0);
}

void sys_write(char *buff)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_write */
    invoke_syscall(SYSCALL_WRITE, (long)buff, 0, 0, 0, 0);
}

void sys_reflush(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_reflush */
    invoke_syscall(SYSCALL_REFLUSH, 0, 0, 0, 0, 0);
}

int sys_mutex_init(int key)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_init */
    long rtval = invoke_syscall(SYSCALL_LOCK_INIT, (long)key, 0, 0, 0, 0);
    return (int)rtval;
}

void sys_mutex_acquire(int mutex_idx)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_acquire */
    invoke_syscall(SYSCALL_LOCK_ACQ, (long)mutex_idx, 0, 0, 0, 0);
}

void sys_mutex_release(int mutex_idx)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_release */
    invoke_syscall(SYSCALL_LOCK_RELEASE, (long)mutex_idx, 0, 0, 0, 0);
}

long sys_get_timebase(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_timebase */
    long rtval = invoke_syscall(SYSCALL_GET_TIMEBASE, 0, 0, 0, 0, 0);
    return rtval;
}

long sys_get_tick(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_tick */
    long rtval = invoke_syscall(SYSCALL_GET_TICK, 0, 0, 0, 0, 0);
    return rtval;
}

void sys_sleep(uint32_t time)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_sleep */
    invoke_syscall(SYSCALL_SLEEP, (long)time, 0, 0, 0, 0);
}

void sys_thread_yield()
{
    /* [p2-task5] thread_yield */
    invoke_syscall(SYSCALL_THREAD_YIELD, 0, 0, 0, 0, 0);
}

void sys_thread_create(uint64_t entry_addr, uint64_t arg){
    /* [p2-task5] thread_create*/
    invoke_syscall(SYSCALL_THREAD_CREATE, entry_addr, arg, 0, 0, 0);
}
/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/