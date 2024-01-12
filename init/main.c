#include <common.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50

#define TMP_BUF     1000
#define TASKNUM_LOC 0x502001f2

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];

// [p1-task4] task num
short task_num;
int app_info_offset, os_size;

static int bss_check(void)
{
    for (int i = 0; i < VERSION_BUF; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
}

/* [p1-task4] Personal Tool: print integer */
static void my_print_int(int val){
    if (val < 0){
        bios_putchar('-');
        val = -val;
    }
    if (val < 10){
        bios_putchar('0' + val);
        return;
    }else{
        my_print_int(val / 10);
        bios_putchar(val % 10 + '0');
    }
    return;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    short *tasknum_ptr  = (short *)TASKNUM_LOC;
    int *offset_ptr     = (int *)(TASKNUM_LOC + 2);

    char tmp_app_info[TMP_BUF];

    // loading task num and kernel size
    task_num            = *tasknum_ptr;
    app_info_offset     = *offset_ptr;

    bios_putstr("============================================================\n\r");
    bios_putstr("\tUser APP Info: \n\r");
    bios_putstr("\t\tTask Num: ");
    my_print_int((int)task_num);
    bios_putstr("\n\r\n\r");

    bios_putstr("\t\tAPP-Info Offset: ");
    my_print_int(app_info_offset);
    bios_putstr(" bytes\n\r");
    bios_putstr("============================================================\n\r");

    // loading APP Info to taskinfo[]
    task_info_t *task_info_ptr;
    int task_info_size = task_num * sizeof(task_info_t);
    int task_info_sec_id = app_info_offset / SECTOR_SIZE;
    int task_info_sec_num = NBYTES2SEC(app_info_offset + task_info_size) - task_info_sec_id;
    bios_sd_read(tmp_app_info, task_info_sec_num, task_info_sec_id);

    task_info_ptr = (task_info_t *)(tmp_app_info + app_info_offset - SECTOR_SIZE * task_info_sec_id);
    memcpy((uint8_t *)tasks, (uint8_t *)task_info_ptr, task_num * sizeof(task_info_t));
}

/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

int main(void)
{
    // Check whether .bss section is set to zero
    int check = bss_check();

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Output 'Hello OS!', bss check result and OS version
    char output_str[] = "bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i)
    {
        buf[i] = output_str[i];
        if (buf[i] == '_')
        {
            buf[i] = output_val[output_val_pos++];
        }
    }

    bios_putstr("Hello OS!\n\r");
    bios_putstr(buf);

    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.

    /* [p1-task4] load vias task name */
        bios_putstr("Input task name: \n\r");
        char ch;
        char task_name_buf[TASK_NAME_LEN];
        int name_idx = 0;
        while (1){
            ch = port_read_ch();
            if (ch == 255)
                continue;
            else{
                if (ch == '\r'){
                    bios_putstr("\n\r");
                    task_name_buf[name_idx] = '\0';
                    bios_putstr("\n\r============================================================\n\r");
                    bios_putstr("Loading Task via name[");
                    bios_putstr(task_name_buf);
                    bios_putstr("]\n\r");

                    unsigned func_addr = load_task_img(task_name_buf);
                    if (func_addr != 0){
                       void (*func_pointer)() = func_addr;
                        (*func_pointer)(); 
                    }
                    else{
                        bios_putstr("Load Task fail.\n\r");
                    }
                    name_idx = 0;
                    bios_putstr("============================================================\n\r");
                    bios_putstr("\n\rInput task name: \n\r");
                }
                else{
                    bios_putchar(ch);
                    task_name_buf[name_idx] = ch;
                    name_idx++;
                }
            }
        }
    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        asm volatile("wfi");
    }

    return 0;
}
