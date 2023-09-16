#include <asm/biosdef.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

// [p1-task5] ker_decompressor address
.equ ker_decmp_entry, 0x50230000

.text
.global main

main:
	// fence on all memory and I/O
	fence

	// TODO: [p1-task1] call BIOS to print string "It's bootblock!"
	li		a7, BIOS_PUTSTR
	la		a0, msg
	jal		bios_func_entry

/*
	// TODO: [p1-task2] call BIOS to read kernel in SD card
	li		a7, BIOS_SDREAD					# func label

	lui		a0, %hi(kernel)					# load kernel addr
	addi	a0, a0, %lo(kernel)

	la		t0, os_size_loc					# load block num
	lh		a1, 0(t0)

	li		a2, 1							# load start id

	jal		bios_func_entry					# call BIOS to read kernel in SD card

	// TODO: [p1-task4] load task-related arguments and pass them to kernel


	// TODO: [p1-task2] jump to kernel to start UCAS-OS
	j		kernel
*/
	// [p1-task5] call BIOS to load ker_decompressor
	li		a7, BIOS_SDREAD

	la		a0, ker_decmp_entry

	la		t0, os_size_loc
	lh		a1, -8(t0)

	li		a2, 1

	jal		bios_func_entry

	jal		ker_decmp_entry

	j		kernel

	/************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a bootloader...\n\r"

