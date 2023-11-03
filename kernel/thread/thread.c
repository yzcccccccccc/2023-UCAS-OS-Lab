#include <csr.h>
#include <os/irq.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/_thread.h>
#include <os/smp.h>
#include <printk.h>

extern void ret_from_exception();

/*************************************************
    Actually we use PCB as TCB.
**************************************************/
void init_tcb(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb, uint64_t arg
){
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    for (int i = 0; i < 32; i++)
        pt_regs->regs[i] = 0;
    pt_regs->regs[2] = (reg_t)user_stack;       // sp
    pt_regs->regs[4] = (reg_t)pcb;              // tp
    pt_regs->regs[10] = (reg_t)arg;             // a0 (passing args!)
    pt_regs->sepc = (reg_t)entry_point;
    pt_regs->sstatus = SR_SPIE & ~SR_SPP;

    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    for (int i = 0; i < 14; i++)
        pt_switchto->regs[i] = 0;
    pcb->kernel_sp = (reg_t)pt_switchto;
    pt_switchto->regs[1] = (reg_t)pt_switchto;          // sp
    pt_switchto->regs[0] = (reg_t)ret_from_exception;   // ra
}

pid_t thread_create(uint64_t entry_addr, uint64_t arg){
    // [p3-multicore]
    int cpuid = get_current_cpu_id();

    if (pid_n > NUM_MAX_TASK)
        return -1;                      // create fail.
    pid_n++;
    pcb_t* pcb_new = pcb + pid_n;
    pcb_new->kernel_sp = allocKernelPage(1) + PAGE_SIZE;
    pcb_new->user_sp = allocUserPage(1) + PAGE_SIZE;
    pcb_new->pid = pid_n;
    pcb_new->status = TASK_READY;
    pcb_new->cursor_x = pcb_new->cursor_y = 0;
    pcb_new->par = current_running[cpuid];
    pcb_new->tid = current_running[cpuid]->pid;
    pcb_new->thread_type = SUB_THREAD;
    strcpy(pcb_new->name, "thread-sub");
    list_insert(&ready_queue, &(pcb_new->list));
    init_tcb(pcb_new->kernel_sp, pcb_new->user_sp, entry_addr, pcb_new, arg);
    return pid_n;
}

void thread_yield(){
    // [p3-multicore]
    int cpuid = get_current_cpu_id();

    list_node_t *head_ptr = &ready_queue;
    list_node_t *tmp_ptr = head_ptr->next;
    pcb_t *cur = (pcb_t *)((void *)tmp_ptr - LIST_PCB_OFFSET);
    pcb_t *prev = current_running[cpuid];
    if (list_empty(head_ptr)){
        printk("Thread yield fail: Empty ready queue.\n");
        do_scheduler();
    }

    while (tmp_ptr != head_ptr && cur->tid != current_running[cpuid]->tid){
        tmp_ptr = tmp_ptr->next;
        cur = (pcb_t *)((void *)tmp_ptr - LIST_PCB_OFFSET);
    }

    if (tmp_ptr == head_ptr){
        printk("Thread yild fail: No ready grouped thread.\n");
        do_scheduler();
    }
    else{
        list_delete(tmp_ptr);
        if (prev->status == TASK_RUNNING){
            prev->status = TASK_READY;
            list_insert(&ready_queue, &(prev->list));
        }
        cur->status = TASK_RUNNING;
        cur->cid    = cpuid;
        process_id[cpuid] = cur->pid;
        current_running[cpuid] = cur;
        switch_to(prev, current_running[cpuid]);
    }
}