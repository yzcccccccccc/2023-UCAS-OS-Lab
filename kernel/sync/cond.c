#include <os/sync.h>

condition_t conds[CONDITION_NUM];

void init_conditions(){
    for (int i = 0; i < CONDITION_NUM; i++){
        conds[i].key = -1;
        conds[i].wait_queue.next = conds[i].wait_queue.prev = &(conds[i].wait_queue);
    }
}

int do_condition_init(int key){
    int cond_idx = -1;
    for (int i = 0; i < CONDITION_NUM; i++){
        if (conds[i].key == -1){
            cond_idx = i;
            conds[i].key = key;
        }
    }
    return cond_idx;
}

void do_condition_wait(int cond_idx, int mutex_idx){
    do_mutex_lock_release(mutex_idx);           // prev release
    do_block(&(current_running->list), &(conds[cond_idx].wait_queue));
    do_mutex_lock_acquire(mutex_idx);           // next acquire
}

void do_condition_signal(int cond_idx){
    list_node_t *ptr = list_pop(&(conds[cond_idx].wait_queue));
    pcb_t *pcb_ptr;
    while (ptr != NULL){
        pcb_ptr = (pcb_t *)((void *)ptr - LIST_PCB_OFFSET);
        if (pcb_ptr->status == TASK_BLOCKED)
            break;
        ptr = list_pop(&(conds[cond_idx].wait_queue));
    }
    if (ptr != NULL){
        do_unblock(ptr);
    }
}

void do_condition_broadcast(int cond_idx){
    list_node_t *ptr = list_pop(&(conds[cond_idx].wait_queue));
    pcb_t *pcb_ptr;
    while (!list_empty(&(conds[cond_idx].wait_queue))){
        if (ptr != NULL){
            pcb_ptr = (pcb_t *)((void *)ptr - LIST_PCB_OFFSET);
            if (pcb_ptr->status == TASK_BLOCKED)
                do_unblock(ptr);
        }
        ptr = list_pop(&(conds[cond_idx].wait_queue));
    }
}

void do_condition_destroy(int cond_idx){
    conds[cond_idx].key = -1;
    do_condition_broadcast(cond_idx);
}