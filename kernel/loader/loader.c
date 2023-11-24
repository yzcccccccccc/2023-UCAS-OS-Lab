#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <type.h>

uint64_t load_task_img(char *taskname, pcb_t *pcb_ptr)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    /* [p1-task4] load via task name */
    int task_size, task_offset;
    int st_sec_id, occ_sec_num;                 // start sector id and occupied sectors
    uint64_t task_addr = 0;
    uint64_t task_memsz;
    uint64_t pgdir = pcb_ptr->pgdir;

    // 1 sector = 512 Bytes, 1 page = 4KB, 1 page = 8 sectors

    for (int i = 0; i < task_num; i++){
        if (strcmp(taskname, tasks[i].task_name) == 0){
            task_addr  = tasks[i].p_vaddr;
            task_memsz = tasks[i].p_memsz;

            task_size   = tasks[i].size;
            task_offset = tasks[i].offset;
            st_sec_id   = task_offset / SECTOR_SIZE;
            occ_sec_num = NBYTES2SEC(task_offset + task_memsz) - st_sec_id;

            // load the img every 4KB
            uint64_t va = task_addr, kva, st_kva = 0;
            for (int sec_to_read = occ_sec_num, cur_sec_id = st_sec_id; 
                    sec_to_read > 0; 
                    sec_to_read -= 8, va += 0x1000, cur_sec_id += 8){
                kva = alloc_page_helper(va, pgdir, pcb_ptr, PF_UNPINNED);
                if (st_kva == 0)
                    st_kva = kva;
                unsigned int cur_sec_to_read = (sec_to_read > 8) ? 8 : sec_to_read;
                // load a page into physical memory
                bios_sd_read((unsigned int)kva2pa(kva), cur_sec_to_read, cur_sec_id);
                // set up the map on the swap area
                allocPage_from_freeSF(pcb_ptr, get_vf(va));
            }

            //bios_sd_read(task_addr, occ_sec_num, st_sec_id);        // rough transporting!

            // fine transporting !
            // pay attention that task_addr is in (another process's) user pgtable, so we need to use kernel virual address.
            // consequent? - nope!, use (another process's) virtual addr, then convert it into kva
            char *app_ptr, *head_ptr;
            uint64_t head_uva, app_uva;
            head_uva    = task_addr;
            app_uva     = head_uva + task_offset - st_sec_id * SECTOR_SIZE;
            for (int j = 0; j < task_memsz; j++){
                head_ptr = (char *)get_kva_v(head_uva, pgdir);
                app_ptr  = (char *)get_kva_v(app_uva, pgdir);
                *head_ptr = *app_ptr;
                head_uva++;
                app_uva++;
            }

            /*
            int *app_ptr, *head_ptr;
            int ITE_BOUND = task_memsz / sizeof(int);
            head_ptr = (int *)st_kva;
            app_ptr = (int *)(st_kva + task_offset - st_sec_id * SECTOR_SIZE);
            for (int j = 0; j < ITE_BOUND; j++){
                *head_ptr = *app_ptr;
                head_ptr++;
                app_ptr++;
            }*/
                
            break;
        }
    }
    return task_addr;
}