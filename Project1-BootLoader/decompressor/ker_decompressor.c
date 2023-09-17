/*
    [p1-task5] 
        Self-Decompressor for kernel
    Created by Yzcc
*/
#include "tinylibdeflate.h"
#include <common.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/string.h>
#include <os/task.h>

#define RESTORE_LOC         0x50201000
#define OS_SIZE_LOC         0x502001fc

#define MAX_AVAIL_BYTES     100000

char compressed[MAX_AVAIL_BYTES];

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
}

int main(){
    char *rst_ptr = (char *)RESTORE_LOC;
    char *ptr;

    // initiate jmp_tab
    init_jmptab();

    bios_putstr("\n\r\t\tDecompressing ...\n\r");

    // prepare environment
    struct libdeflate_decompressor *decompressor = deflate_alloc_decompressor();

    // load and decompress
    int restore_nbytes = 0;

    int *size_ptr = (int *)(OS_SIZE_LOC);
    int os_size = *size_ptr;

    int *offset_ptr = (int *)(OS_SIZE_LOC - 4);
    int os_offset = *offset_ptr;

    int os_st_sec   = os_offset / SECTOR_SIZE;
    int os_sectors  = NBYTES2SEC(os_size + os_offset) - os_st_sec;

    bios_sd_read(compressed, os_sectors, os_st_sec);               // Rough Load

    ptr = (compressed + os_offset % SECTOR_SIZE);                      // Fine Load
    for (int i = 0; i < os_size; i++){
        compressed[i] = *ptr;
        ptr++;
    }

    int rtval = deflate_deflate_decompress(decompressor, compressed, os_size, rst_ptr, MAX_AVAIL_BYTES, &restore_nbytes);

    if (rtval){
        bios_putstr("\t\tAn error occurred during decompression. :(\n\r");
        while (1){
            asm volatile("wfi");
        }
    }
    else{
        bios_putstr("\t\tSuccessfully decompress. :)\n\r\n\r");
    }
    return 0;
}