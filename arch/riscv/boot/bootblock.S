#include <asm/biosdef.h>
#include <csr.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50202000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

.text
.global main

main:
	// fence on all memory and I/O
	fence
	csrw 	CSR_SIE, zero				# mask interrupt in CSR_SIE

    // a0 is mhartid
	bnez a0, secondary

	// TODO: [p1-task1] call BIOS to print string "It's bootblock!"
	li		a7, BIOS_PUTSTR
	la		a0, msg
	jal		bios_func_entry

	// TODO: [p1-task2] call BIOS to read kernel in SD card
	li		a7, BIOS_SDREAD					# func label

	lui		a0, %hi(kernel)					# load kernel addr
	addi	a0, a0, %lo(kernel)
	mv		s0, a0

	la		t0, os_size_loc					# load block num
	lh		s1, 0(t0)
	li		s2, 1							# load start id
	li		s3, 64

kernel_loader:
	mv		a1, s1
	blt		s1, s3, call_bios_loader		# branch if s1 < 64
	mv		a1, s3
call_bios_loader:
	mv		a0, s0
	mv		a2, s2
	li		a7, BIOS_SDREAD
	jal		bios_func_entry					# call BIOS to read kernel in SD card
	addi	s2, s2, 64						# s2 = s2 + 64
	addi	s1, s1, -64						# s1 = s1 - 64
	li		t0, 0x8000
	add		s0, s0, t0						# 64 sectors = 0x8000 bytes
	blt		zero, s1, kernel_loader
	

	// TODO: [p1-task4] load task-related arguments and pass them to kernel


	// TODO: [p1-task2] jump to kernel to start UCAS-OS
	j		kernel

	/************************************************************/
secondary:
	/* TODO [P3-task3]: 
	 * 1. Mask all interrupts
	 * 2. let stvec pointer to kernel_main
	 * 3. enable software interrupt for ipi
	 */
	csrw 	CSR_SIE, zero				# mask interrupt in CSR_SIE
	li    	t0, SR_SIE
  	csrs  	CSR_SSTATUS, t0

	la		t0, kernel
	csrw	CSR_STVEC, t0

	li		t0, SIE_SSIE
	csrs	CSR_SIE, t0


wait_for_wakeup:
	wfi
	j wait_for_wakeup

	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a bootloader...\n\r"

