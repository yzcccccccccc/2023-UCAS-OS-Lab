#include <os/sync.h>

semaphore_t semas[SEMAPHORE_NUM];

void init_semaphores(){
    for (int i = 0; i < SEMAPHORE_NUM; i++){
        semas[i].wait_queue.next = semas[i].wait_queue.prev = &(semas[i].wait_queue);
        semas[i].key = -1;             // Unallocated :)
    }
}

int do_semaphore_init(int key, int init){
    int sema_id = -1;
    for (int i = 0; i < SEMAPHORE_NUM; i++){
        if (semas[i].key == -1){
            semas[i].key = key;
            semas[i].num = init;
            sema_id = i;
            break;
        }
    }
    return sema_id;
}

void do_semaphore_up(int sema_idx){
    semas[sema_idx].num++;
    if (semas[sema_idx].num <= 0){
        // Wake up one process in the waiting queue
        list_node_t *ptr = list_pop(&(semas[sema_idx].wait_queue));
        if (ptr != NULL)
            do_unblock(ptr);
    }
}

void do_semaphore_down(int sema_idx){
    semas[sema_idx].num--;
    if (semas[sema_idx].num < 0){
        // Block current process
        do_block(&(current_running->list), &(semas[sema_idx].wait_queue));
    }
}

void do_semaphore_destroy(int sema_idx){
    semas[sema_idx].key = -1;
    // Release the wait queue
    list_node_t *ptr;
    while (!list_empty(&(semas[sema_idx].wait_queue))){
        ptr = list_pop(&(semas[sema_idx].wait_queue));
        do_unblock(ptr);
    }
}