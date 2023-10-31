#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
    # clear .bss
    la    t0, __bss_start           # start addr of .bss
    la    t1, __BSS_END__           # end addr of .bss
Loop:
    blt   t1, t0, EOLoop
    sb    x0, 0(t0)
    addi  t0, t0, 1
    j     Loop
EOLoop:

    # create stack frame
    addi    sp, sp, -8
    sd      ra, 0(sp)

    /* TODO: [p1-task3] enter main function */
    jal     main

    /************************************************************/
    /* TODO: [P3] call sys_exit */
    jal     sys_exit
    /************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
