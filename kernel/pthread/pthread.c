#include <csr.h>
#include <os/irq.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/pthread.h>
#include <os/smp.h>
#include <printk.h>

extern void ret_from_exception();

void init_tcb(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb, void *arg
){
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    for (int i = 0; i < 32; i++)
        pt_regs->regs[i] = 0;
    pt_regs->regs[2] = (reg_t)user_stack;       // sp
    pt_regs->regs[4] = (reg_t)pcb;              // tp
    pt_regs->regs[10] = (reg_t)arg;             // a0 (passing args!)
    pt_regs->sepc = (reg_t)entry_point;
    pt_regs->sstatus = (SR_SPIE | SR_SUM) & ~SR_SPP;

    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    for (int i = 0; i < 14; i++)
        pt_switchto->regs[i] = 0;
    pcb->kernel_sp = (reg_t)pt_switchto;
    pt_switchto->regs[1] = (reg_t)pt_switchto;          // sp
    pt_switchto->regs[0] = (reg_t)ret_from_exception;   // ra
}

// [p4-task4] create a thread
pid_t pthread_create(uint64_t entry_addr, void *arg){
    int cpuid = get_current_cpu_id();

    // Step1: Find an available pcb
    int avail_idx = 0;
    for (int i = 1; i < NUM_MAX_TASK; i++){
        if (pcb[i].status == TASK_EXITED || pcb[i].status == TASK_UNUSED){
            avail_idx = i;
            break;
        }
    }
    if (!avail_idx)
        return 0;                       // allocate fail

    // Step2: Update pcb info
    pid_n++;
    pcb_t* pcb_new = pcb + avail_idx;
    // page dir
    pcb_new->pgdir = current_running[cpuid]->pgdir;             // copy the page
    // pid
    pcb_new->pid = pid_n;
    // thread info
    pcb_new->tid = current_running[cpuid]->tid + 1;
    pcb_new->thread_type = SUB_THREAD;
    pcb_new->par = current_running[cpuid];
    // cpu
    pcb_new->mask = current_running[cpuid]->mask;
    // status
    pcb_new->status = TASK_READY;
    list_insert(&ready_queue, &(pcb_new->list));
    // list
    pcb_new->wait_list.next = pcb_new->wait_list.prev = &(pcb_new->wait_list);
    pcb_new->pf_list.next = pcb_new->pf_list.prev = &(pcb_new->pf_list);
    pcb_new->sf_list.next = pcb_new->sf_list.prev = &(pcb_new->sf_list);
    // cursor
    pcb_new->cursor_x = pcb_new->cursor_y = 0;
    // name
    strcpy(pcb_new->name, current_running[cpuid]->name);
    

    // Step3: allocate stack
    // Kernel stack
    if (pcb_new->status == TASK_UNUSED)
        pcb_new->kernel_stack_base = pcb_new->kernel_sp = allocPage(1) + NORMAL_PAGE_SIZE;
    else
        pcb_new->kernel_sp = pcb_new->kernel_stack_base;
    uint64_t kernel_stack_kva = pcb_new->kernel_sp;
    // User stack
    uint64_t user_stack_uva = USER_STACK_ADDR + NORMAL_PAGE_SIZE * pcb_new->tid * 2;
    pcb_new->user_sp = pcb_new->user_stack_base = user_stack_uva;
    alloc_page_helper(user_stack_uva - NORMAL_PAGE_SIZE, pcb_new->pgdir, pcb_new, PF_UNPINNED);
    allocPage_from_freeSF(pcb_new, get_vf(user_stack_uva - NORMAL_PAGE_SIZE));

    // Step4: init stack
    init_tcb(kernel_stack_kva, user_stack_uva, entry_addr, pcb_new, arg);

    return pid_n;
}

// [p4-task4] thread join
int pthread_join(pid_t thread_id){
    int cpuid = get_current_cpu_id();
    pid_t rtval = 0;
    for (int i = 1; i < NUM_MAX_TASK; i++){
        if (pcb[i].pid == thread_id){
            rtval = thread_id;
            if (pcb[i].status != TASK_EXITED)
                do_block(&(current_running[cpuid]->list), &(pcb[i].wait_list));
            break;
        }
    }
    return rtval;
}