#include <os/cmd.h>
#include <os/task.h>
#include <printk.h>
#include <os/string.h>
#include <os/smp.h>
#include <os/mm.h>

#define NAME_LEN_SPACE 20

/* [p3] for ps cmd in shell */
void do_process_show(){
    printk("[Progress Table :D]\n");

    /* Table Head */
    printk("PID");
    for (int i = 0; i < 5; i++)     printk(" ");
    printk("Name");
    for (int i = 0; i < NAME_LEN_SPACE - 4; i++)    printk(" ");
    printk("Status ");
    printk("       CPUID");
    printk("       MASK");
    printk("       PAR_PID\n");

    /* Table Body */
    for (int i = 1, len; i < TASK_MAXNUM; i++){
        if (pcb[i].pid == 0 || pcb[i].status == TASK_EXITED)
            continue;
        // pid
        printk("%03d", pcb[i].pid);
        for (int j = 0; j < 5; j++) printk(" ");

        // name
        len = strlen(pcb[i].name);
        printk("%s", pcb[i].name);
        for (int j = 0; j < NAME_LEN_SPACE - len; j++)  printk(" ");

        // status
        printk("%s", (pcb[i].status == TASK_BLOCKED ? "BLOCKED"
                : pcb[i].status == TASK_READY ? "READY  "
                : pcb[i].status == TASK_RUNNING ? "RUNNING"
                : pcb[i].status == TASK_EXITED ? "EXITED "
                : "ZOMBIED"));

        // CPUID
        if (pcb[i].status == TASK_RUNNING){
            printk("       %d", pcb[i].cid);
            if (pcb[i].cid / 10)
                printk("   ");
            else
                printk("    ");
        }
        else{
            printk("       N/A");
            printk("  ");
        }
        
        // mask
        printk("       0x%x ", pcb[i].mask);

        // par pid
        if (pcb[i].par == NULL)
            printk("       N/A\n");
        else
            printk("       %d\n", pcb[i].par->pid);
    }
}

// [p4] personel: memory show
#define SCALER_NUM      50
#define SCALE           2           // 100/SCALER_NUM
void do_memory_show(){
    printk("[Memory :D] \n");
    int percent, scale_percent;

    // Physical Pages
    percent = (1000 * (NUM_MAX_PHYPAGE - free_page_num) / NUM_MAX_PHYPAGE);
    scale_percent = percent / 10;
    printk("Physical    Pages: [");
    for (int i = 0; i < SCALER_NUM; i++){
        if (i * SCALE < scale_percent)
            printk("|");
        else
            printk(" ");
    }
    printk("] %d.%d%%\n", percent / 10, percent % 10);

    // Swap Pages
    percent = (1000 * (NUM_MAX_SWPPAGE - free_swp_page_num) / NUM_MAX_SWPPAGE);
    scale_percent = percent / 10;
    printk("Swap        Pages: [");
    for (int i = 0; i < SCALER_NUM; i++){
        if (i * SCALE < scale_percent)
            printk("|");
        else
            printk(" ");
    }
    printk("] %d.%d%%\n", percent / 10, percent % 10);

    // Shared Pages
    percent = (1000 * (NUM_MAX_SHMPAGE - free_shm_page_num) / NUM_MAX_SHMPAGE);
    scale_percent = percent / 10;
    printk("Shared      Pages: [");
    for (int i = 0; i < SCALER_NUM; i++){
        if (i * SCALE < scale_percent)
            printk("|");
        else
            printk(" ");
    }
    printk("] %d.%d%%\n", percent / 10, percent % 10);
    return;
}

pid_t do_exec(char *name, int argc, char *argv[]){
    pid_t pid = init_pcb_vname(name, argc, argv);
    return pid;
}

pid_t do_getpid(){
    // [p3-multicore]
    int cpuid = get_current_cpu_id();
    
    return current_running[cpuid]->pid;
}

int do_taskset(char *name, int mask, int pid){
    int rtval = 0;
    if (name == NULL){              // has arg '-p'
        for (int i = 0; i < NUM_MAX_TASK; i++)
            if (pcb[i].pid == pid){
                pcb[i].mask = mask;
                rtval = 1;
                break;
            }
    }
    else{
        char *argv[1] = {name};
        int new_pid = init_pcb_vname(name, 1, argv);
        for (int i = 0; i < NUM_MAX_TASK; i++)
            if (pcb[i].pid == new_pid){
                pcb[i].mask = mask;
                break;
            }
        rtval = new_pid;
    }
    return rtval;
}