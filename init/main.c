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
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>

extern void ret_from_exception();

// Task info array
task_info_t tasks[TASK_MAXNUM];
// [p1-task4] task num
short task_num, app_info_offset, os_size;

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
    short *info_ptr = (short *)0x502001fa;
    unsigned tmp_app_info_addr = 0x52000000;

    // loading task num and kernel size
    task_num = *(info_ptr);
    os_size = *(info_ptr + 1);
    app_info_offset = *(info_ptr + 2);

    bios_putstr("=======================================\n\r");
    bios_putstr("\tTask Num: ");
    my_print_int((int)task_num);
    bios_putstr("\n\r\n\r");

    bios_putstr("\tOS Size: ");
    my_print_int((int)os_size);
    bios_putstr(" sectors\n\r\n\r");

    bios_putstr("\tAPP-Info Offset: ");
    my_print_int((int)app_info_offset);
    bios_putstr(" bytes\n\r");
    bios_putstr("=======================================\n\r");

    // loading APP Info to taskinfo[]
    task_info_t *task_info_ptr;
    int task_info_size = task_num * sizeof(task_info_t);
    int task_info_sec_id = app_info_offset / SECTOR_SIZE;
    int task_info_sec_num = NBYTES2SEC(app_info_offset + task_info_size) - task_info_sec_id;
    bios_sd_read(tmp_app_info_addr, task_info_sec_num, task_info_sec_id);

    task_info_ptr = (task_info_t *)(tmp_app_info_addr + app_info_offset - SECTOR_SIZE * task_info_sec_id);
    memcpy((uint8_t *)tasks, (uint8_t *)task_info_ptr, task_num * sizeof(task_info_t));
}

/************************************************************/
static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
     /* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));


    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    for (int i = 0; i < 14; i++)
        pt_switchto->regs[i] = 0;
    pcb->kernel_sp = (reg_t)pt_switchto;
    pt_switchto->regs[1] = pcb->kernel_sp;              // sp
    pt_switchto->regs[0] = (reg_t)entry_point;          // ra

}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    for (int i = 0; i < task_num; i++){
        if (!strcmp(tasks[i].task_name, "print1") || !strcmp(tasks[i].task_name, "print2") || !strcmp(tasks[i].task_name, "fly")
            || !strcmp(tasks[i].task_name, "lock1") || !strcmp(tasks[i].task_name, "lock2")){
            ptr_t entry_point = (load_task_img(tasks[i].task_name));
            pid_n++;

            pcb_t* pcb_new = pcb + pid_n;
            pcb_new->kernel_sp = allocKernelPage(1) + PAGE_SIZE;
            pcb_new->user_sp = allocUserPage(1) + PAGE_SIZE;
            pcb_new->pid = pid_n;
            pcb_new->status = TASK_READY;
            pcb_new->cursor_x = pcb_new->cursor_y = 0;
            strcpy(pcb_new->name, tasks[i].task_name);
            list_insert(&ready_queue, &pcb_new->list);

            init_pcb_stack(pcb_new->kernel_sp, pcb_new->user_sp, entry_point, pcb_new);
        }
    }

    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running = &pid0_pcb;
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
}
/************************************************************/

int main(void)
{
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

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    
    
    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        // enable_preempt();
        // asm volatile("wfi");
    }

    return 0;
}
