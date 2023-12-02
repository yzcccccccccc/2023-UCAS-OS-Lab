#include <os/irq.h>
#include <os/time.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/kernel.h>
#include <os/smp.h>
#include <os/mm.h>
#include <printk.h>
#include <assert.h>
#include <screen.h>
#include <csr.h>

handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];

void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    local_flush_icache_all();
    local_flush_tlb_all();

    // TODO: [p2-task3] & [p2-task4] interrupt handler.
    // call corresponding handler by the value of `scause`
    uint64_t irq_flag = scause & SCAUSE_IRQ_FLAG;
    uint64_t func_code = scause & ~SCAUSE_IRQ_FLAG;
    handler_t handler;
    
    int cpuid = get_current_cpu_id();
    // check status (may be killed by another core)
    if (current_running[cpuid]->status == TASK_EXITED || current_running[cpuid]->status == TASK_ZOMBIE){
        do_scheduler();
    }
    else {
        if (irq_flag){                                  // interrupt
            handler = irq_table[func_code];
        }
        else{                                           // exception
            handler = exc_table[func_code];
        }

        handler(regs, stval, scause);

        // [p4]
        local_flush_tlb_all();
        local_flush_icache_all();
    }
}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task4] clock interrupt handler.
    // Note: use bios_set_timer to reset the timer and remember to reschedule
    bios_set_timer(get_ticks() + TIMER_INTERVAL);
    do_scheduler();
}

/****************************************************************************
    handle page fault, also handle the stack overflow here (core dumped)
    User Stack Location:
    ----------------------------------------------------
                    ...
    0xf00014000 ---------------------------------------- > Sub thread 2 (tid=2)
                        Sub stack area (4KB)
    0xf00013000 ----------------------------------------
                        Critical area (4KB)
    0xf00012000 ---------------------------------------- > Sub thread 1 (tid=1)
                        Sub stack area (4KB)
    0xf00011000 ----------------------------------------
                        Critical area (4KB)
    0xf00010000 ---------------------------------------- > main thread (tid0)
                        User Stack Area (8KB)
    0xf0000e000 ----------------------------------------
                        Critical Area (4KB)
    0xf0000d000 ----------------------------------------
                (below are heap, txt, etc.)
    ----------------------------------------------------
    When entering the critical area, we assume that an overflow occur, and 
    then we will kill the process.
*****************************************************************************/
void handle_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause){
    int cpuid = get_current_cpu_id();
    // get the range of the critical area
    uint64_t lower_bound, upper_bound;
    if (current_running[cpuid]->thread_type == MAIN_THREAD){
        lower_bound = USER_CRITICAL_AREA_LOWER;
        upper_bound = USER_CRITICAL_AREA_UPPER;
    }
    else{
        lower_bound = USER_CRITICAL_AREA_LOWER + (current_running[cpuid]->tid - 1) * NORMAL_PAGE_SIZE * 2 + NORMAL_PAGE_SIZE * 3;
        upper_bound = lower_bound + NORMAL_PAGE_SIZE;
    }
    if (stval >= lower_bound && stval < upper_bound){     // in the critical area!
        printk("[Core %d]: Core dumped at: 0x%lx, pid = %d\n", cpuid, stval, current_running[cpuid]->pid);
        do_exit();
    }      
    else{
        uint64_t attribute = _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_READ | _PAGE_WRITE;
        uint64_t va = get_vf(stval);                        // belongs to which virtual frame?
        pcb_t *pcb_ptr = current_running[cpuid];
        swp_pg_t *swp_pg_ptr = query_swp_page(va, pcb_ptr);     // sub-thread stack?
        if (pcb_ptr->par != NULL && swp_pg_ptr == NULL){    // main-thread
            pcb_ptr = pcb_ptr->par;
            swp_pg_ptr = query_swp_page(va, pcb_ptr);
        }

        if (swp_pg_ptr == NULL){                                // not in swap area -> alloc a new page
            uint64_t va = get_vf(stval);
            if (va >= SHM_PAGE_BASE && va < SHM_PAGE_BOUND){    // occupy shared memory va?
                int idx = (va - SHM_PAGE_BASE) / NORMAL_PAGE_SIZE;
                current_running[cpuid]->shm_info |= (1 << idx);
            }
            alloc_page_helper(stval, pcb_ptr, PF_UNPINNED, attribute);              // alloc a physical page
            allocPage_from_freeSF(pcb_ptr, stval, attribute);                       // copy to swap area
        }
        else{                                               // in the swap area, just swap in
            swap_in(swp_pg_ptr);
        }
    }
    return;
}

// [p4] core0 swap out core1's page
void handle_ipi(regs_context_t *regs, uint64_t stval, uint64_t scause){
    local_flush_icache_all();
    local_flush_tlb_all();
    clear_SIP();
    return;
}

void handle_irq_ext(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p5-task3] external interrupt handler.
    // Note: plic_claim and plic_complete will be helpful ...
}

void init_exception()
{
    /* TODO: [p2-task3] initialize exc_table */
    /* NOTE: handle_syscall, handle_other, etc.*/
    for (int i = 0; i < EXCC_COUNT; i++){
        exc_table[i] = handle_other;
    }
    exc_table[EXCC_SYSCALL] = handle_syscall;
    exc_table[EXCC_INST_PAGE_FAULT] = exc_table[EXCC_LOAD_PAGE_FAULT] = exc_table[EXCC_STORE_PAGE_FAULT] = handle_page_fault;
    /* TODO: [p2-task4] initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/
    for (int i = 0; i < IRQC_COUNT; i++){
        irq_table[i] = handle_other;
    }
    irq_table[IRQC_S_TIMER] = handle_irq_timer;
    irq_table[IRQC_S_SOFT] = handle_ipi;
    /* TODO: [p2-task3] set up the entrypoint of exceptions */
    setup_exception();
}

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    int cpuid = get_current_cpu_id();
    printk("cpuid: %d\n", cpuid);

    char* reg_name[] = {
        "zero "," ra  "," sp  "," gp  "," tp  ",
        " t0  "," t1  "," t2  ","s0/fp"," s1  ",
        " a0  "," a1  "," a2  "," a3  "," a4  ",
        " a5  "," a6  "," a7  "," s2  "," s3  ",
        " s4  "," s5  "," s6  "," s7  "," s8  ",
        " s9  "," s10 "," s11 "," t3  "," t4  ",
        " t5  "," t6  "
    };
    for (int i = 0; i < 32; i += 3) {
        for (int j = 0; j < 3 && i + j < 32; ++j) {
            printk("%s : %016lx ",reg_name[i+j], regs->regs[i+j]);
        }
        printk("\n\r");
    }
    printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r",
           regs->sstatus, regs->sbadaddr, regs->scause);
    printk("sepc: 0x%lx\n\r", regs->sepc);
    printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
    assert(0);
}
