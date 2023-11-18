#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <os/_thread.h>
#include <os/cmd.h>
#include <os/sync.h>
#include <os/smp.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>

extern void ret_from_exception();
spin_lock_t unmap_sync_lock;

// Task info array
task_info_t tasks[TASK_MAXNUM];
// [p1-task4] task num
short task_num, os_size;
int app_info_offset;

int pid_n;

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[SD_WRITE]        = (long (*)())sd_write;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
    jmptab[YIELD]           = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;

    // TODO: [p2-task1] (S-core) initialize system call table.
    jmptab[WRITE]           = (long (*)())screen_write;
    jmptab[REFLUSH]         = (long (*)())screen_reflush;

}

/* [p1-task4] Personal Tool: print integer */
static void my_print_int(int val){
    if (val < 0){
        bios_putchar('-');
        val = -val;
    }
    if (val < 10){
        bios_putchar('0' + val);
        return;
    }else{
        my_print_int(val / 10);
        bios_putchar(val % 10 + '0');
    }
    return;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    task_info_t tasks_tmp[TASK_MAXNUM];
    ptr_t info_ptr = 0x502001f8;                            // [p4] Physical Addr
    ptr_t tmp_app_info_addr = (ptr_t)tasks_tmp;             // [p4] Virtual Addr

    // loading task num and kernel size
    app_info_offset = *((int *)pa2kva(info_ptr));
    os_size = *((short *)pa2kva(info_ptr + 4));
    task_num = *((short *)pa2kva(info_ptr + 6));

    bios_putstr("=======================================================================\n\r");
    bios_putstr("\tTask Num: ");
    my_print_int((int)task_num);
    bios_putstr("\n\r\n\r");

    bios_putstr("\tOS Size: ");
    my_print_int((int)os_size);
    bios_putstr(" sectors\n\r\n\r");

    bios_putstr("\tAPP-Info Offset: ");
    my_print_int((int)app_info_offset);
    bios_putstr(" bytes\n\r");
    bios_putstr("=======================================================================\n\r");

    // loading APP Info to taskinfo[]
    task_info_t *task_info_ptr;
    int task_info_size = task_num * sizeof(task_info_t);
    int task_info_sec_id = app_info_offset / SECTOR_SIZE;
    int task_info_sec_num = NBYTES2SEC(app_info_offset + task_info_size) - task_info_sec_id;
    bios_sd_read(kva2pa(tmp_app_info_addr), task_info_sec_num, task_info_sec_id);

    task_info_ptr = (task_info_t *)(tmp_app_info_addr + app_info_offset - SECTOR_SIZE * task_info_sec_id);
    memcpy((uint8_t *)tasks, (uint8_t *)task_info_ptr, task_num * sizeof(task_info_t));
}

/************************************************************/
static void init_pcb(void)
{   
    /* [p3] init pcb[i]'s status to be exited */
    for (int i = 0; i < NUM_MAX_TASK; i++)
        pcb[i].status = TASK_UNUSED;

    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    char name[] = "shell";
    char *fake_argv[1];
    fake_argv[0] = (char *)(&name);
    int shell_pid = init_pcb_vname(name, 1, fake_argv);
    pcb[shell_pid].mask = 0x3;                  // both core can exec

    /* TODO: [p2-task1] remember to initialize 'current_running' */
    pid0_core0_pcb.status = TASK_RUNNING;
    current_running[0] = &pid0_core0_pcb;
    asm volatile ("mv tp, %0\n\t"
                :
                :"r"(current_running[0])
                :"tp"
                );
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    syscall[SYSCALL_SLEEP]              = (long (*)())do_sleep;
    syscall[SYSCALL_YIELD]              = (long (*)())do_scheduler;
    syscall[SYSCALL_WRITE]              = (long (*)())screen_write;
    syscall[SYSCALL_CURSOR]             = (long (*)())screen_move_cursor;
    syscall[SYSCALL_REFLUSH]            = (long (*)())screen_reflush;
    syscall[SYSCALL_GET_TIMEBASE]       = (long (*)())get_time_base;
    syscall[SYSCALL_GET_TICK]           = (long (*)())get_ticks;
    syscall[SYSCALL_LOCK_INIT]          = (long (*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ]           = (long (*)())do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE]       = (long (*)())do_mutex_lock_release;
    syscall[SYSCALL_THREAD_CREATE]      = (long (*)())thread_create;
    syscall[SYSCALL_THREAD_YIELD]       = (long (*)())thread_yield;

    syscall[SYSCALL_CURSOR_C]           = (long (*)())screen_move_cursor_c;
    syscall[SYSCALL_READCH]             = (long (*)())bios_getchar;
    syscall[SYSCALL_CLEAR]              = (long (*)())screen_clear;
    syscall[SYSCALL_PS]                 = (long (*)())do_process_show;
    syscall[SYSCALL_EXEC]               = (long (*)())do_exec;
    syscall[SYSCALL_EXIT]               = (long (*)())do_exit;
    syscall[SYSCALL_KILL]               = (long (*)())do_kill;
    syscall[SYSCALL_WAITPID]            = (long (*)())do_waitpid;
    syscall[SYSCALL_GETPID]             = (long (*)())do_getpid;

    syscall[SYSCALL_SEMA_INIT]          = (long (*)())do_semaphore_init;
    syscall[SYSCALL_SEMA_UP]            = (long (*)())do_semaphore_up;
    syscall[SYSCALL_SEMA_DOWN]          = (long (*)())do_semaphore_down;
    syscall[SYSCALL_SEMA_DESTROY]       = (long (*)())do_semaphore_destroy;
    syscall[SYSCALL_BARR_INIT]          = (long (*)())do_barrier_init;
    syscall[SYSCALL_BARR_WAIT]          = (long (*)())do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY]       = (long (*)())do_barrier_destroy;
    syscall[SYSCALL_COND_INIT]          = (long (*)())do_condition_init;
    syscall[SYSCALL_COND_WAIT]          = (long (*)())do_condition_wait;
    syscall[SYSCALL_COND_SIGNAL]        = (long (*)())do_condition_signal;
    syscall[SYSCALL_COND_BROADCAST]     = (long (*)())do_condition_broadcast;
    syscall[SYSCALL_COND_DESTROY]       = (long (*)())do_condition_destroy;
    syscall[SYSCALL_MBOX_CLOSE]         = (long (*)())do_mbox_close;
    syscall[SYSCALL_MBOX_OPEN]          = (long (*)())do_mbox_open;
    syscall[SYSCALL_MBOX_RECV]          = (long (*)())do_mbox_recv;
    syscall[SYSCALL_MBOX_SEND]          = (long (*)())do_mbox_send;

    syscall[SYSCALL_TASKSET]            = (long (*)())do_taskset;
}
/************************************************************/

int main(void)
{   
    if (get_current_cpu_id()){
        /**************************************************
                Wake up sub core :D
        **************************************************/
        lock_kernel();

        setup_exception();
        bios_set_timer(get_ticks() + TIMER_INTERVAL);

        pid0_core1_pcb.status = TASK_RUNNING;
        current_running[1] = &pid0_core1_pcb;
        asm volatile ("mv tp, %0\n\t"
                :
                :"r"(current_running[1])
                :"tp"
                );

        // [p4-task1] unmap!
        unmap();
        spin_lock_release(&unmap_sync_lock);

        printk("> [INIT] Sub core initialization succeeded. :D\n");

        unlock_kernel();
        while (1)
        {
            // If you do non-preemptive scheduling, it's used to surrender control
            //do_scheduler();

            // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
            enable_preempt();
            asm volatile("wfi");
        }
    }
    else{
        /**************************************************
                Wake up main core :D
        **************************************************/
        // Init jump table provided by kernel and bios(ΦωΦ)
        init_jmptab();

        // Init task information (〃'▽'〃)
        init_task_info();

        // Init Process Control Blocks |•'-'•) ✧
        init_pcb();
        printk("> [INIT] PCB initialization succeeded.\n");

        // Read CPU frequency (｡•ᴗ-)_
        time_base = bios_read_fdt(TIMEBASE);

        // Init lock mechanism o(´^｀)o
        init_locks();
        printk("> [INIT] Lock mechanism initialization succeeded.\n");

        // Init Synchronization :D
        init_semaphores();
        init_barriers();
        init_conditions();
        init_mbox();
        printk("> [INIT] Synchronization initialization succeeded.\n");

        // Init interrupt (^_^)
        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n");

        // Init system call table (0_0)
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");

        // [p3-multicore] init kernel_lock
        smp_init();
        printk("> [INIT] kernel_lock initialization succeeded.\n");

        // Init screen (QAQ)
        init_screen();
        printk("> [INIT] SCREEN initialization succeeded.\n");

        // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
        // NOTE: The function of sstatus.sie is different from sie's
        bios_set_timer(get_ticks() + TIMER_INTERVAL);

        printk("> [INIT] Main core initialization succeeded. :D\n");

        // [p3-multicore] wakeup sub core :P
        wakeup_other_hart();

        // [p4-unmap] cancel the map of 0x50200000 ~ 0x51000000, done by sub-core
        spin_lock_init(&unmap_sync_lock);
        spin_lock_try_acquire(&unmap_sync_lock);            // lock, wait core1 unlock it
        spin_lock_acquire(&unmap_sync_lock);

        // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
        while (1)
        {
            // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
            enable_preempt();
            asm volatile("wfi");
        }
    }
    return 0;
}
