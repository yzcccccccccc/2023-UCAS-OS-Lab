/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000
#define PAGE_SIZE       4096

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  /* [p3-multicore] only main core will do the dirty work :D */
  csrr  t0, CSR_MHARTID
  bnez  t0, secondary

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  la    t0, __bss_start           # start addr of .bss
  la    t1, __BSS_END__           # end addr of .bss
Loop:
  blt   t1, t0, EOLoop
  sb    x0, 0(t0)
  addi  t0, t0, 1
  j     Loop
EOLoop:

  /* TODO: [p1-task2] setup C environment */
  la    sp, KERNEL_STACK          # set stack-pointer to KERNEL_STACK
  j     main

secondary:
  la    sp, KERNEL_STACK
  la    t0, PAGE_SIZE
  add   sp, sp, t0
  j     main

loop:
  wfi
  j loop

END(_start)
