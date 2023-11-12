#include <os/task.h>
#include <os/loader.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/smp.h>
#include <csr.h>

extern void ret_from_exception();

void init_pcb_stack(ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point, pcb_t *pcb,
        int argc, char *argv[]){
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    for (int i = 0; i < 32; i++)
        pt_regs->regs[i] = 0;
    pt_regs->regs[4] = (reg_t)pcb;              // tp
    pt_regs->regs[10] = (reg_t)argc;            // a0

    pt_regs->sepc = (reg_t)entry_point;
    pt_regs->sstatus = SR_SPIE & ~SR_SPP;

    /* [p3] passing argv content! */
    char **pt_argv = (char **)(user_stack - (argc + 1) * 8);
    ptr_t user_sp = (ptr_t)pt_argv;
    for (int i = 0; i < argc; i++){
        user_sp -= (strlen(argv[i]) + 1);
        strcpy((char *)user_sp, argv[i]);
        pt_argv[i] = (char *)user_sp;
    }
    pt_argv[argc] = NULL;

    pt_regs->regs[11] = (reg_t)pt_argv;     // a1

    user_sp = ROUNDDOWN(user_sp, 16);       // 128 bits = 16 bytes
    pt_regs->regs[2] = (reg_t)user_sp;       // sp


    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
    * NOTE: you should prepare a stack, and push some values to
    * simulate a callee-saved context.
    */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    for (int i = 0; i < 14; i++)
        pt_switchto->regs[i] = 0;
    pcb->kernel_sp = (reg_t)pt_switchto;
    pcb->user_sp = (reg_t)user_sp;
    pt_switchto->regs[1] = (reg_t)pt_switchto;          // sp
    pt_switchto->regs[0] = (reg_t)ret_from_exception;   // ra
}

pid_t init_pcb_vname(char *name, int argc, char *argv[]){
    int load_suc = 0;
    pid_t alloc_pcb_idx = 0;
    int cpuid = get_current_cpu_id();

    /* [p3] allocate a pcb block */
    for (int i = 1; i < NUM_MAX_TASK; i++){
        if (pcb[i].status == TASK_EXITED || pcb[i].status == TASK_UNUSED){
            alloc_pcb_idx = i;
            break;
        }
    }
    if (!alloc_pcb_idx)
        return 0;               // allocated fail.

    ptr_t entry_point = (load_task_img(name));
    if (entry_point){
        load_suc = 1;
        pid_n++;

        pcb_t* pcb_new = pcb + alloc_pcb_idx;
        if (pcb_new->status == TASK_UNUSED){
            pcb_new->kernel_sp  = allocKernelPage(1) + PAGE_SIZE;
            pcb_new->user_sp    = allocUserPage(1) + PAGE_SIZE;
            pcb_new->kernel_stack_base  = pcb_new->kernel_sp;
            pcb_new->user_stack_base    = pcb_new->user_sp;
        }
        else{
            pcb_new->kernel_sp  = pcb_new->kernel_stack_base;
            pcb_new->user_sp    = pcb_new->user_stack_base;
        }
       
        pcb_new->pid = pid_n;
        pcb_new->status = TASK_READY;
        pcb_new->cursor_x = pcb_new->cursor_y = 0;

        // for thread
        pcb_new->tid = pid_n;
        pcb_new->thread_type = MAIN_THREAD;

        // [p3] wait_list init
        pcb_new->wait_list.next = pcb_new->wait_list.prev = &(pcb_new->wait_list);

        // [p3] CPU mask
        if (current_running[cpuid] == NULL)
            pcb_new->mask = 0x3;                            // both cores can execute
        else
            pcb_new->mask = current_running[cpuid]->mask;   // inherit

        strcpy(pcb_new->name, name);
        list_insert(&ready_queue, &pcb_new->list);

        init_pcb_stack(pcb_new->kernel_sp, pcb_new->user_sp, entry_point, pcb_new, argc, argv);
    }
    else {
        load_suc = 0;
    }
    return pid_n * load_suc;
}