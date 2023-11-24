#include <os/task.h>
#include <os/loader.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/smp.h>
#include <printk.h>
#include <csr.h>

extern void ret_from_exception();

// [p4-task3] argc, argv may be swapped out ...
// abandoned, because argc is passed by regs, and argv will be on the security page

/**************************************************************************
    Kernel Stack:
    ______________________________________________________________________
    (kva)kernel_stack/pcb->kernel_stack:    -----------------------------
                                                (sizeof regs_contxt)
    (kva)pt_regs:                           -----------------------------
                                                (sizeof switch_contxt)
    (kva)pt_switchto:                       -----------------------------
    ______________________________________________________________________
    User Stack:
    ______________________________________________________________________
    (kva)user_stack/(uva)pcb->user_stack:   -----------------------------
                                                (sizeof *argv[])
    (kva)pt_argv:                           -----------------------------
                                                (sizeof strings and
                                                alignment.)
    (kva)user_sp(final):                    -----------------------------
    ______________________________________________________________________

***************************************************************************/

// kernel_stack & user_stack: use kernel virtual address !
void init_pcb_stack(ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point, pcb_t *pcb,
        int argc, char *argv[]){
    /* [p2] regs_contxt, saving regs when interruptted, on the kernel stack */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    for (int i = 0; i < 32; i++)
        pt_regs->regs[i] = 0;
    pt_regs->regs[4] = (reg_t)pcb;              // tp
    pt_regs->regs[10] = (reg_t)argc;            // a0

    pt_regs->sepc = (reg_t)entry_point;
    pt_regs->sstatus = (SR_SPIE | SR_SUM) & ~SR_SPP;

    /* [p3] passing argv content! on the user_stack */
    char **pt_argv = (char **)(user_stack - (argc + 1) * 8);
    ptr_t user_sp = (ptr_t)pt_argv;
    for (int i = 0; i < argc; i++){
        user_sp -= (strlen(argv[i]) + 1);
        strcpy((char *)user_sp, argv[i]);
        pt_argv[i] = (char *)(pcb->user_sp - (user_stack - user_sp));                   // save as uva
    }
    pt_argv[argc] = NULL;

    // pay attention that pcb->user_sp is the user virtual addr !
    pt_regs->regs[11]   = (reg_t)(pcb->user_sp - (user_stack - (ptr_t)pt_argv));        // a1, addr of *argv[]
    user_sp = ROUNDDOWN(user_sp, 16);                                                   // 128 bits = 16 bytes
    pt_regs->regs[2]    = (reg_t)(pcb->user_sp - (user_stack - user_sp));               // sp

    //uint64_t tmp_kva1 = get_kva_v(pt_regs->regs[11], pcb->pgdir);
    //uint64_t tmp_kva2 = get_kva_v(pt_regs->regs[2], pcb->pgdir);


    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
    * NOTE: you should prepare a stack, and push some values to
    * simulate a callee-saved context.
    */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    for (int i = 0; i < 14; i++)
        pt_switchto->regs[i] = 0;
    pcb->kernel_sp = (reg_t)pt_switchto;
    pcb->user_sp = (reg_t)(pcb->user_sp - (user_stack - user_sp));
    pt_switchto->regs[1] = (reg_t)pt_switchto;                      // sp
    pt_switchto->regs[0] = (reg_t)ret_from_exception;               // ra
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
        return 0;               // allocate fail.

    pcb_t* pcb_new = pcb + alloc_pcb_idx;
    uint64_t pgdir = 0;
    // [p4] allocate the pgdir
    if (pcb_new->status == TASK_UNUSED){       // first allocated
        pgdir = allocPage(1);                  // directly alloc, cause this is root page table, may be reused
        share_pgtable(pgdir, pa2kva(PGDIR_PA));
        pcb_new->pgdir = pgdir;
    }
    else
        pgdir = pcb_new->pgdir;

    // [p3] wait_list init
    pcb_new->wait_list.next = pcb_new->wait_list.prev = &(pcb_new->wait_list);

    // [p4] pf_list & sf_list init
    pcb_new->pf_list.next = pcb_new->pf_list.prev = &(pcb_new->pf_list);
    pcb_new->sf_list.next = pcb_new->sf_list.prev = &(pcb_new->sf_list);

    // [p4] pid
    pid_n++;
    pcb_new->pid = pid_n;

    ptr_t entry_point = (load_task_img(name, pcb_new));
    if (entry_point){
        uint64_t kernel_stack_kva, user_stack_kva;
        load_suc = 1;
        
        // alloc a page for kernel stack
        if (pcb_new->status == TASK_UNUSED){
            pcb_new->kernel_stack_base = pcb_new->kernel_sp = allocPage(1) + NORMAL_PAGE_SIZE;
        }
        else{
            pcb_new->kernel_sp  = pcb_new->kernel_stack_base;
        }
        kernel_stack_kva = pcb_new->kernel_sp;

        // [p4] alloc a page for user stack
        pcb_new->user_sp = pcb_new->user_stack_base = USER_STACK_ADDR;
        user_stack_kva = alloc_page_helper(USER_STACK_ADDR - NORMAL_PAGE_SIZE, pgdir, pcb_new, PF_UNPINNED) + NORMAL_PAGE_SIZE;
        allocPage_from_freeSF(pcb_new, get_vf(USER_STACK_ADDR - NORMAL_PAGE_SIZE));

        // [p4] alloc a security page
        alloc_page_helper(SECURITY_BASE, pgdir, pcb_new, PF_PINNED);
        allocPage_from_freeSF(pcb_new, SECURITY_BASE);
       
        pcb_new->status = TASK_READY;
        pcb_new->cursor_x = pcb_new->cursor_y = 0;

        // for thread
        pcb_new->tid = 0;
        pcb_new->par = NULL;
        pcb_new->thread_type = MAIN_THREAD;

        // [p3] CPU mask
        if (current_running[cpuid] == NULL)
            pcb_new->mask = 0x3;                            // both cores can execute
        else
            pcb_new->mask = current_running[cpuid]->mask;   // inherit

        strcpy(pcb_new->name, name);
        list_insert(&ready_queue, &pcb_new->list);

        init_pcb_stack(kernel_stack_kva, user_stack_kva, entry_point, pcb_new, argc, argv);
    }
    else {
        load_suc = 0;
        pid_n--;
    }
    return pid_n * load_suc;
}