#include <common.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];

// [p1-task4] task num
short task_num, kernel_size, os_size;

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
    while (val){
        bios_putchar('0' + val % 10);
        val /= 10;
    }
    return;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    short *info_ptr = (short *)0x502001fa;

    // loading task num and kernel size
    task_num = *(info_ptr);
    os_size = *(info_ptr + 1);
    kernel_size = *(info_ptr + 2);

    bios_putstr("=======================================\n\r");
    bios_putstr("Task Num: ");
    my_print_int((int)task_num);
    bios_putstr("\n\r\n\r");

    bios_putstr("OS Size: ");
    my_print_int((int)os_size);
    bios_putstr("\n\r\n\r");

    bios_putstr("Kernel Size: ");
    my_print_int((int)kernel_size);
    bios_putstr("\n\r\n\r");
    bios_putstr("=======================================\n\r");

    // loading APP Info to taskinfo[]
    task_info_t *task_info_ptr;
    task_info_ptr = (task_info_t *)(0x50200200 + kernel_size);
    memcpy(tasks, task_info_ptr, task_num * sizeof(task_info_t));
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

    /* [p1-task2] echo! 
        char ch;
        while (1){
            ch = port_read_ch();
            if (ch == 255)
                continue;
            else{
                if (ch == '\r')
                    bios_putstr("\n\r");
                else
                    bios_putchar(ch);
            }
        }
    */

    /* [p1-task3] load via task id 
        bios_putstr("Input task id:\n\r");
        int task_id = 0;
        char ch;
        while (1){
            ch = port_read_ch();
            if (ch == 255)
                continue;
            else{
                if (ch == '\r'){
                    bios_putstr("\n\r");
                    if (0 <= task_id && task_id <= 3){
                        bios_putstr("Loading task...\n\r");
                        unsigned func_addr = load_task_img(task_id);
                        void (*func_pointer)() = func_addr;
                        (*func_pointer)();
                    }
                    else{
                        bios_putstr("Invalid task id! \n\r");
                    }
                    task_id = 0;
                }
                else{
                    bios_putchar(ch);
                    task_id = task_id * 10 + ch - '0';
                }
            }
        }
    */

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
                    task_name_buf[i] = '\0';
                    bios_putstr("Loading Task via name[");
                    bios_putstr(task_name_buf);
                    bios_putstr("]\n\r");

                    unsigned func_addr = load_task_img(task_name_buf);
                    if (func_addr != 0){
                       void (*func_pointer)() = func_addr;
                        (*func_pointer)(); 
                    }
                    else{
                        bios_putstr("Unknown Task!\n\r");
                    }
                    name_idx = 0;
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
