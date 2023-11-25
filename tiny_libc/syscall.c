#include <syscall.h>
#include <stdint.h>
#include <kernel.h>
#include <unistd.h>
#include <stddef.h>
#include <security_page.h>

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

void sys_move_cursor_c(int dx, int dy)
{
    /* [p3] move cursor comparatively */
    invoke_syscall(SYSCALL_CURSOR_C, (long)dx, (long)dy, 0, 0, 0);
}

void sys_write(char *buff)
{
    /* [p4-task3] copy parameters to security page */
    RESET_SECPAGE_PTR;
    buff = (char *)copy_str_to_secPage(buff);

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

int sys_thread_create(uint64_t entry_addr, void *arg){
    /* [p2-task5 & p4-task4] thread_create*/
    return invoke_syscall(SYSCALL_THREAD_CREATE, entry_addr, (long)arg, 0, 0, 0);
}

int sys_thread_join(int thread_id){
    /* [p5-task4] */
    return invoke_syscall(SYSCALL_THREAD_JOIN, (long)thread_id, 0, 0, 0, 0);
}

void sys_clear(){
    /* [p3] for shell 'clear' command */
    invoke_syscall(SYSCALL_CLEAR, 0, 0, 0, 0, 0);
}

/************************************************************/
#ifdef S_CORE
pid_t  sys_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exec for S_CORE */
}    
#else
pid_t  sys_exec(char *name, int argc, char **argv)
{
    /* [p4-task3] */
    RESET_SECPAGE_PTR;
    name = (char *)copy_str_to_secPage(name);
    argv = (char **)copy_argv_to_secPage(argv, argc);

    /* TODO: [p3-task1] call invoke_syscall to implement sys_exec */
    return invoke_syscall(SYSCALL_EXEC, (long)name, (long)argc, (long)argv, 0, 0);
}
#endif

void sys_exit(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exit */
    invoke_syscall(SYSCALL_EXIT, 0, 0, 0, 0, 0);
}

int  sys_kill(pid_t pid)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_kill */
    return invoke_syscall(SYSCALL_KILL, (long)pid, 0, 0, 0, 0);
}

int  sys_waitpid(pid_t pid)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_waitpid */
    return invoke_syscall(SYSCALL_WAITPID, (long)pid, 0, 0, 0, 0);
}


void sys_ps(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_ps */
    invoke_syscall(SYSCALL_PS, 0, 0, 0, 0, 0);
}

pid_t sys_getpid()
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getpid */
    return invoke_syscall(SYSCALL_GETPID, 0, 0, 0, 0, 0);
}

int  sys_getchar(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getchar */
    return invoke_syscall(SYSCALL_READCH, 0, 0, 0, 0, 0);
}

int  sys_barrier_init(int key, int goal)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrier_init */
    return invoke_syscall(SYSCALL_BARR_INIT, (long)key, (long)goal, 0, 0, 0);
}

void sys_barrier_wait(int bar_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_wait */
    invoke_syscall(SYSCALL_BARR_WAIT, (long)bar_idx, 0, 0, 0, 0);
}

void sys_barrier_destroy(int bar_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_destory */
    invoke_syscall(SYSCALL_BARR_DESTROY, (long)bar_idx, 0, 0, 0, 0);
}

int sys_condition_init(int key)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_init */
    return invoke_syscall(SYSCALL_COND_INIT, (long)key, 0, 0, 0, 0);
}

void sys_condition_wait(int cond_idx, int mutex_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_wait */
    invoke_syscall(SYSCALL_COND_WAIT, (long)cond_idx, (long)mutex_idx, 0, 0, 0);
}

void sys_condition_signal(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_signal */
    invoke_syscall(SYSCALL_COND_SIGNAL, (long)cond_idx, 0, 0, 0, 0);
}

void sys_condition_broadcast(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_broadcast */
    invoke_syscall(SYSCALL_COND_BROADCAST, (long)cond_idx, 0, 0, 0, 0);
}

void sys_condition_destroy(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_destroy */
    invoke_syscall(SYSCALL_COND_DESTROY, (long)cond_idx, 0, 0, 0, 0);
}

int sys_semaphore_init(int key, int init)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_init */
    return invoke_syscall(SYSCALL_SEMA_INIT, (long)key, (long)init, 0, 0, 0);
}

void sys_semaphore_up(int sema_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_up */
    invoke_syscall(SYSCALL_SEMA_UP, (long)sema_idx, 0, 0, 0, 0);
}

void sys_semaphore_down(int sema_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_down */
    invoke_syscall(SYSCALL_SEMA_DOWN, (long)sema_idx, 0, 0, 0, 0);
}

void sys_semaphore_destroy(int sema_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_destroy */
    invoke_syscall(SYSCALL_SEMA_DESTROY, (long)sema_idx, 0, 0, 0, 0);
}

int sys_mbox_open(char * name)
{
    /* [p4-task3] */
    RESET_SECPAGE_PTR;
    name = (char *)copy_str_to_secPage(name);

    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_open */
    return invoke_syscall(SYSCALL_MBOX_OPEN, (long)name, 0, 0, 0, 0);
}

void sys_mbox_close(int mbox_id)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_close */
    invoke_syscall(SYSCALL_MBOX_CLOSE, (long)mbox_id, 0, 0, 0, 0);
}

int sys_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    /* [p4-task3] */
    RESET_SECPAGE_PTR;
    msg = (void *)copy_str_to_secPage((char *)msg);

    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_send */
    return invoke_syscall(SYSCALL_MBOX_SEND, (long)mbox_idx, (long)msg, (long)msg_length, 0, 0);
}

int sys_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    /* [p4-task3] */
    RESET_SECPAGE_PTR;
    uint64_t tmp_ptr = secPage_ptr;

    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_recv */
    int rtval = invoke_syscall(SYSCALL_MBOX_RECV, (long)mbox_idx, (long)tmp_ptr, (long)msg_length, 0, 0);

    copy_secPage_to_ptr((void *)tmp_ptr, msg, msg_length);
    return rtval;
}
/************************************************************/

int sys_taskset(char *name, int mask, int pid){
    /* [p4-task3] */
    RESET_SECPAGE_PTR;
    name = (char *)copy_str_to_secPage(name);

    /* [p3-task5] taskset */
    return invoke_syscall(SYSCALL_TASKSET, (long)name, (long)mask, (long)pid, 0, 0);
}

void* sys_shmpageget(int key)
{
    /* TODO: [p4-task4] call invoke_syscall to implement sys_shmpageget */
    return 0;
}

void sys_shmpagedt(void *addr)
{
    /* TODO: [p4-task4] call invoke_syscall to implement sys_shmpagedt */
}
/************************************************************/
