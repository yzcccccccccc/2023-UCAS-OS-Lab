#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <os/string.h>
#include <os/task.h>
#include <os/smp.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>


pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_core0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
const ptr_t pid0_core1_stack = INIT_KERNEL_STACK + 3 * PAGE_SIZE;
pcb_t pid0_core0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)(pid0_core0_stack - PAGE_SIZE),
    .user_sp = (ptr_t)pid0_core0_stack,
    .cid = 0,
    .mask = 0x1,
    .name = "init0"
};
pcb_t pid0_core1_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)(pid0_core1_stack - PAGE_SIZE),
    .user_sp = (ptr_t)pid0_core1_stack,
    .cid = 1,
    .mask = 0x2,
    .name = "init1"
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
pcb_t * volatile current_running[2];

/* global process id */
pid_t process_id[2] = {1, 1};

void do_scheduler(void)
{   
    // [p3-multicore]
    int cpu_id = get_current_cpu_id();

    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    check_sleeping();
    
    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    // TODO: [p2-task1] Modify the current_running pointer.
    list_node_t *ptr = (&ready_queue)->next;
    pcb_t *next;

    // [p3] Look through the ready_queue
    for (;ptr != &ready_queue; ptr = ptr->next){
        next = (pcb_t *)((void *)ptr - LIST_PCB_OFFSET);
        if ((next->mask & (1 << cpu_id))){     // [p3-task4] run on correct cpu :P
            break;
        }
    }

    if (ptr == &ready_queue){           // no available pcb. :(
        ptr = cpu_id ? &pid0_core1_pcb.list : &pid0_core0_pcb.list;
        next = (pcb_t *)((void *)ptr - LIST_PCB_OFFSET);
    }
    else
        list_delete(ptr);               // remove from ready_queu :D
    
    pcb_t *prev = current_running[cpu_id];

    if (prev->status == TASK_RUNNING){
        prev->status = TASK_READY;
        if (prev != &pid0_core0_pcb && prev != &pid0_core1_pcb)
            list_insert(&ready_queue, &(prev->list));
    }
    
    next->status = TASK_RUNNING;
    next->cid = cpu_id;
    process_id[cpu_id] = next->pid;
    current_running[cpu_id] = next;

    // TODO: [p2-task1] switch_to current_running
    switch_to(prev, current_running[cpu_id]);
}

void do_sleep(uint32_t sleep_time)
{
    // [p3-multicore]
    int cpuid = get_current_cpu_id();
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
    current_running[cpuid]->wakeup_time = get_timer() + sleep_time;
    current_running[cpuid]->status = TASK_BLOCKED;
    list_insert(&sleep_queue, &(current_running[cpuid]->list));
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
    // [p3-multicore]
    int cpuid = get_current_cpu_id();
    int rtval = do_kill(current_running[cpuid]->pid);
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
    // [p3-multicore]
    int cpuid = get_current_cpu_id();
    int suc = 0;
    for (int i = 1; i < TASK_MAXNUM; i++){
        if (pcb[i].pid == pid && pcb[i].status != TASK_EXITED){
            suc = 1;

            /* Release waiting list */
            list_node_t *ptr;
            pcb_t *pcb_ptr;
            while (!list_empty(&(pcb[i].wait_list))){
                ptr = list_pop(&(pcb[i].wait_list));
                pcb_ptr = (pcb_t *)((void *)ptr - LIST_PCB_OFFSET);
                if (pcb_ptr->status == TASK_BLOCKED)
                    do_unblock(ptr);
            }

            /* Release locks */
            lock_resource_release(pcb[i].pid);

            /*************************************************
                Hint:
                the benefit of list_node_t: easy to be del :)
                    mutex_block_que, sleep_que, pcb_wait_que,
                cond_wait_que, sema_wait_que ...
                    All can be released ! :D
                (list_node_t only belongs to one of them.)
            *************************************************/
            list_delete(&(pcb[i].list)); 

            pcb[i].status = TASK_EXITED;

            if (pid == current_running[cpuid]->pid){               // suicide :(
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
    // [p3-multicore]
    int cpuid = get_current_cpu_id();
    pid_t rtval = 0;
    for (int i = 1; i <= pid_n; i++){
        if (pcb[i].pid == pid){
            rtval = pid;
            if (pcb[i].status != TASK_EXITED)
                do_block(&(current_running[cpuid]->list), &(pcb[i].wait_list));
            break;
        }
    }
    return rtval;
}