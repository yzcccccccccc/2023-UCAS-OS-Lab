#include <os/cmd.h>
#include <os/task.h>
#include <printk.h>

/* [p3] for ps cmd in shell */
void do_process_show(){
    printk("[Progress Table :D]\n");
    printk("\t\t PID \t\t Name \t\t Status\n");
    for (int i = 1; i <= pid_n; i++){
        printk("\t\t %d \t\t %s \t\t %s\n", pcb[i].pid, pcb[i].name, 
                (pcb[i].status == TASK_BLOCKED ? "BLOCKED"
                : pcb[i].status == TASK_READY ? "READY"
                : pcb[i].status == TASK_RUNNING ? "RUNNING"
                : "EXITED"));
    }
}

pid_t do_exec(char *name, int argc, char *argv[]){
    pid_t pid = init_pcb_vname(name, argc, argv);
    if (pid == 0){
        printk("Unknown task name :(\n");
    }
    else{
        printk("Successfully start process %s, pid = %d :)\n", name, pid);
    }
    return pid;
}