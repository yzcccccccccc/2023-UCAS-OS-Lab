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

#define RESTORE_LOC         0x50201000
#define TEMP_LOC            0x50300000
#define OS_SIZE_BYTES_LOC   0x502001f8

#define MAX_AVAIL_BYTES     100000

int main(){
    char *tmp_ptr = (char *)TEMP_LOC;
    char *rst_ptr = (char *)RESTORE_LOC;

    // prepare environment
    struct libdeflate_decompressor *decompressor = deflate_alloc_decompressor();

    // do decompress
    int restore_nbytes = 0;
    short *data_len_ptr = (short *)(OS_SIZE_BYTES_LOC);
    short data_len = *data_len_ptr;
    if (deflate_deflate_decompress(decompressor, tmp_ptr, (int)data_len, rst_ptr, MAX_AVAIL_BYTES, &restore_nbytes)){
        bios_putstr("An error occurred during decompression. :(\n\r");
        while (1){
            asm volatile("wfi");
        }
    }
    else{
        bios_putstr("Successfully decompress. :)\n\r");
    }
    return 0;
}