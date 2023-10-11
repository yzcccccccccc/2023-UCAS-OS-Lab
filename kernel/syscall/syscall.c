#include <sys/syscall.h>

long (*syscall[NUM_SYSCALLS])();

void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    /* TODO: [p2-task3] handle syscall exception */
    /**
     * HINT: call syscall function like syscall[fn](arg0, arg1, arg2),
     * and pay attention to the return value and sepc
     */

    uint64_t sys_code = regs->regs[17];     // a7
    long (*sys_func)() = syscall[sys_code];

    uint64_t arg0 = regs->regs[10];         // a0
    uint64_t arg1 = regs->regs[11];         // a1
    uint64_t arg2 = regs->regs[12];         // a2
    uint64_t arg3 = regs->regs[13];         // a3
    uint64_t arg4 = regs->regs[14];         // a4
    long rtval = sys_func(arg0, arg1, arg2, arg3, arg4);

    regs->regs[10] = rtval;                 // a0
    regs->sepc = regs->sepc + 4;            // sepc
}
