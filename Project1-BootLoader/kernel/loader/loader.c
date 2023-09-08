#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

uint64_t load_task_img(char *taskname)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    /* [p1-task3] load via task id 
        unsigned mem_addr = 0x52000000 + taskid * 0x10000;
        unsigned block_id = taskid * 15 + 16;
        bios_sd_read(mem_addr, 15, block_id);
        return mem_addr;
    */

    /* [p1-task4] load via task name */
        bios_putstr("=================================================\n\r");
        int task_size, task_offset, check_mark;
        int st_sec_id, occ_sec_num;                 // start sector id and occupied sectors
        unsigned task_addr;

        check_mark = 0;
        for (int i = 0; i < task_num; i++){
            bios_putstr("Task Checking Now: ");
            bios_putstr(tasks[i].task_name);
            bios_putstr("\n\r");
            if (strcmp(taskname, tasks[i].task_name) == 0){
                check_mark = 1;
                bios_putstr("Task check-in:");
                bios_putstr(taskname);
                bios_putstr(", transporting to memory ...\n\r");

                task_size = tasks[i].size;
                task_offset = tasks[i].offset;
                st_sec_id = task_offset / SECTOR_SIZE;
                occ_sec_num = NBYTES2SEC(task_size);
                task_addr = TASK_MEM_BASE + i * TASK_SIZE;

                bios_sd_read(task_addr, occ_sec_num + 1, st_sec_id);        // rough transporting!

                char *app_ptr, *head_ptr;                                   // fine transporting!
                head_ptr = task_addr;
                app_ptr = task_addr + task_offset - st_sec_id * SECTOR_SIZE;
                for (int j = 0; j < task_size; j++){
                    *head_ptr = *app_ptr;
                    head_ptr++;
                    app_ptr++;
                }
                
                bios_putstr("Loading Task Complete.\n\r");
                bios_putstr("=================================================\n\r");
            }
        }
        return task_addr;
}