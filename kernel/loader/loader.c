#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>
#include "tinylibdeflate.h"

#define COMPR_BUF_LEN       100000
#define MAX_AVAIL_BYTES     100000

char compressed[COMPR_BUF_LEN];

uint64_t load_task_img(char *taskname)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    /* [p1-task5] load compressed APP ! */
        bios_putstr("************************************************************\n\r");
        int task_size, task_offset;
        int st_sec_id, occ_sec_num;                 // start sector id and occupied sectors
        unsigned task_addr = 0;

        // prepare environment
        struct libdeflate_decompressor *decompressor = deflate_alloc_decompressor();

        for (int i = 0; i < task_num; i++){
            if (strcmp(taskname, tasks[i].task_name) == 0){
                bios_putstr("\tTask check-in: [");
                bios_putstr(taskname);
                bios_putstr("], transporting to memory ...\n\r");

                task_size = tasks[i].size;
                task_offset = tasks[i].offset;
                st_sec_id = task_offset / SECTOR_SIZE;
                occ_sec_num = NBYTES2SEC(task_offset + task_size) - st_sec_id;
                task_addr = TASK_MEM_BASE + i * TASK_SIZE;

                bios_sd_read(compressed, occ_sec_num, st_sec_id);            // rough transporting!

                char *app_ptr, *head_ptr;                                   // fine transporting!
                head_ptr = compressed;
                app_ptr = compressed + task_offset % SECTOR_SIZE;
                for (int j = 0; j < task_size; j++){
                    *head_ptr = *app_ptr;
                    head_ptr++;
                    app_ptr++;
                }

                int restore_nbytes;
                int rtval = deflate_deflate_decompress(decompressor, compressed, task_size, (void *)task_addr, MAX_AVAIL_BYTES, &restore_nbytes);

                if (rtval){
                    bios_putstr("\tDecompress Error :(\n\r");
                    task_addr = 0;
                }
                else{
                    bios_putstr("\tLoading Task Complete :)\n\r");
                }  
                
                bios_putstr("************************************************************\n\r");
                break;
            }
        }
        return task_addr;
}