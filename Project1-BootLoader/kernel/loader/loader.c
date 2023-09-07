#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

uint64_t load_task_img(int taskid)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    /* [p1-task3] load via task id */
    unsigned mem_addr = 0x52000000 + taskid * 0x10000;
    unsigned block_id = taskid * 15 + 16;
    bios_sd_read(mem_addr, 15, block_id);
    return mem_addr;
}