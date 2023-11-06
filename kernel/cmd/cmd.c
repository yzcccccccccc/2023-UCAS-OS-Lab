#include <os/cmd.h>
#include <os/task.h>
#include <printk.h>
#include <os/string.h>
#include <os/smp.h>

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
    printk("       MASK\n");

    /* Table Body */
    for (int i = 1, len; i <= pid_n; i++){
        if (pcb[i].pid == 0)
            continue;
        printk("%03d", pcb[i].pid);
        for (int j = 0; j < 5; j++) printk(" ");

        len = strlen(pcb[i].name);
        printk("%s", pcb[i].name);
        for (int j = 0; j < NAME_LEN_SPACE - len; j++)  printk(" ");

        printk("%s", (pcb[i].status == TASK_BLOCKED ? "BLOCKED"
                : pcb[i].status == TASK_READY ? "READY  "
                : pcb[i].status == TASK_RUNNING ? "RUNNING"
                : "EXITED "));
        if (pcb[i].status == TASK_RUNNING){
            printk("       %d", pcb[i].cid);
            if (pcb[i].cid % 10)
                printk("   ");
            else
                printk("    ");
        }
        else{
            printk("       N/A");
            printk("  ");
        }
        printk("       0x%x\n", pcb[i].mask);
    }
}

pid_t do_exec(char *name, int argc, char *argv[]){
    int cpuid = get_current_cpu_id();
    pid_t pid = init_pcb_vname(name, argc, argv);
    if (pid != 0)
        pcb[pid].mask = current_running[cpuid]->mask;
    return pid;
}

pid_t do_getpid(){
    // [p3-multicore]
    int cpuid = get_current_cpu_id();
    
    return current_running[cpuid]->pid;
}

int do_taskset(char *name, int mask, int pid){
    int rtval;
    if (name == NULL){              // has arg '-p'
        if (pid > 0 && pid <= pid_n){
            pcb[pid].mask = mask;
            rtval = 1;
        }
        else {
            rtval = 0;
        }
    }
    else{
        char *argv[1] = {name};
        int new_pid = init_pcb_vname(name, 1, argv);
        pcb[new_pid].mask = mask;
        rtval = new_pid;
    }
    return rtval;
}