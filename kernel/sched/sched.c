#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <os/string.h>
#include <os/task.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>


pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack,
    .name = "init"
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
pcb_t * volatile current_running;

/* global process id */
pid_t process_id = 1;

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    check_sleeping();
    
    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    // TODO: [p2-task1] Modify the current_running pointer.
    if (list_empty(&ready_queue))
        return;
    
    list_node_t *next_node = list_pop(&ready_queue);
    pcb_t *prev = current_running;
    pcb_t *next = (pcb_t *)((void *)next_node - LIST_PCB_OFFSET);

    //printk("Switch: [%d]%s -> [%d]%s\r\n", prev->pid, prev->name, next->pid, next->name);

    if (prev->status == TASK_RUNNING){
        prev->status = TASK_READY;
        list_insert(&ready_queue, &(prev->list));
    }

    next->status = TASK_RUNNING;
    process_id = next->pid;
    current_running = next;

    // TODO: [p2-task1] switch_to current_running
    switch_to(prev, current_running);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
    current_running->wakeup_time = get_timer() + sleep_time;
    current_running->status = TASK_BLOCKED;
    list_insert(&sleep_queue, &(current_running->list));
    do_scheduler();
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    pcb_t *block_pcb_ptr = (pcb_t *)((void *)pcb_node - LIST_PCB_OFFSET);
    block_pcb_ptr->status = TASK_BLOCKED;
    list_insert(queue, pcb_node);
    do_scheduler();
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    pcb_t *unblock_pcb_ptr = (pcb_t *)((void *)pcb_node - LIST_PCB_OFFSET);

    /* [p3] Maybe first we need to check the dependency(waiting list)... */
    unblock_pcb_ptr->status = TASK_READY;
    list_insert(&ready_queue, pcb_node);
}


/* [p3] exit, kill, waitpid */
void do_exit(){                                         /* Exit current-running */
    int rtval = do_kill(current_running->pid);
    if (!rtval){
        printk("\n[Error] Fail to Exit. Please Restart. \n");
        while (1){
            asm volatile("wfi");
        }
    }
    else
        do_scheduler();
}

int do_kill(pid_t pid){                                 /* Kill process of certain pid */
    int suc = 0;
    for (int i = 1; i <= pid_n; i++){
        if (pcb[i].pid == pid && pcb[i].status != TASK_EXITED){
            suc = 1;

            /* Release waiting list */
            list_node_t *ptr;
            while (!list_empty(&(pcb[i].wait_list))){
                ptr = list_pop(&(pcb[i].wait_list));
                do_unblock(ptr);
            }

            /* Release locks */
            lock_resource_release(pcb[i].pid);

            if (pcb[i].status == TASK_READY)
                list_delete(&(pcb[i].list));
            pcb[i].status = TASK_EXITED;

            if (pid == current_running->pid){               // suicide :(
                do_scheduler();
            }

            if (!strcmp("shell", pcb[i].name)){             // shell will revive :)
                init_pcb_vname("shell", 0, NULL);
            }
            break;
        }
    }
    return suc;
}

int do_waitpid(pid_t pid){
    pid_t rtval = 0;
    for (int i = 1; i <= pid_n; i++){
        if (pcb[i].pid == pid){
            rtval = pid;
            if (pcb[i].status != TASK_EXITED)
                do_block(&(current_running->list), &(pcb[i].wait_list));
            break;
        }
    }
    return rtval;
}