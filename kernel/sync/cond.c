#include <os/sync.h>

condition_t conds[CONDITION_NUM];

void init_conditions(){
    for (int i = 0; i < CONDITION_NUM; i++){
        do_cond_init_ptr(&conds[i]);
        /*conds[i].key = -1;
        conds[i].wait_queue.next = conds[i].wait_queue.prev = &(conds[i].wait_queue);*/
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
    do_cond_wait_ptr(&conds[cond_idx], &mlocks[mutex_idx]);
}

void do_condition_signal(int cond_idx){
    do_cond_signal_ptr(&conds[cond_idx]);
}

void do_condition_broadcast(int cond_idx){
    do_cond_broadcast_ptr(&conds[cond_idx]);
}

void do_condition_destroy(int cond_idx){
    conds[cond_idx].key = -1;
    do_condition_broadcast(cond_idx);
}

/* [p3] ptr version */
void do_cond_wait_ptr(condition_t *cond_ptr, mutex_lock_t *mlock_ptr){
    do_mlock_release_ptr(mlock_ptr);
    do_block(&(current_running->list), &(cond_ptr->wait_queue));
    do_mlock_acquire_ptr(mlock_ptr);
}

void do_cond_signal_ptr(condition_t *cond_ptr){
    list_node_t *ptr = list_pop(&(cond_ptr->wait_queue));
    pcb_t *pcb_ptr;
    while (ptr != NULL){
        pcb_ptr = (pcb_t *)((void *)ptr - LIST_PCB_OFFSET);
        if (pcb_ptr->status == TASK_BLOCKED)
            break;
        ptr = list_pop(&(cond_ptr->wait_queue));
    }
    if (ptr != NULL){
        do_unblock(ptr);
    }
}

void do_cond_broadcast_ptr(condition_t *cond_ptr){
    list_node_t *ptr = list_pop(&(cond_ptr->wait_queue));
    pcb_t *pcb_ptr;
    while (!list_empty(&(cond_ptr->wait_queue))){
        if (ptr != NULL){
            pcb_ptr = (pcb_t *)((void *)ptr - LIST_PCB_OFFSET);
            if (pcb_ptr->status == TASK_BLOCKED)
                do_unblock(ptr);
        }
        ptr = list_pop(&(cond_ptr->wait_queue));
    }
}

void do_cond_init_ptr(condition_t *cond_ptr){
    cond_ptr->wait_queue.next = cond_ptr->wait_queue.prev = &(cond_ptr->wait_queue);
    cond_ptr->key = -1;
}