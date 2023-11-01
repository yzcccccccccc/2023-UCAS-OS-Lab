#include <os/sync.h>

barrier_t barrs[BARRIER_NUM];

void init_barriers(){
    for (int i = 0; i < BARRIER_NUM; i++){
        barrs[i].goal = 0;
        barrs[i].cur_num = 0;
        barrs[i].key = -1;          // unallocated :D
        barrs[i].wait_queue.next = barrs[i].wait_queue.prev = &(barrs[i].wait_queue);
    }
}

int do_barrier_init(int key, int goal){
    int bar_idx = -1;
    for (int i = 0; i < BARRIER_NUM; i++){
        if (barrs[i].key == -1){
            bar_idx = i;
            barrs[i].goal = goal;
            barrs[i].key = key;
            break;
        }
    }
    return bar_idx;
}

void do_barrier_wait(int bar_idx){
    barrs[bar_idx].cur_num++;
    if (barrs[bar_idx].cur_num == barrs[bar_idx].goal){
        // Release the waiting queue !!
        barrs[bar_idx].cur_num = 0;
        list_node_t *ptr;
        while (!list_empty(&(barrs[bar_idx].wait_queue))){
            ptr = list_pop(&(barrs[bar_idx].wait_queue));
            do_unblock(ptr);
        }
    }
    else{
        do_block(&(current_running->list), &(barrs[bar_idx].wait_queue));
    }
}

void do_barrier_destroy(int bar_idx){
    barrs[bar_idx].cur_num = 0;
    barrs[bar_idx].goal = 0;
    barrs[bar_idx].key = -1;
    list_node_t *ptr;
    pcb_t *pcb_ptr;
    while (!list_empty(&(barrs[bar_idx].wait_queue))){
        ptr = list_pop(&(barrs[bar_idx].wait_queue));
        pcb_ptr = (pcb_t *)((void *)ptr - LIST_PCB_OFFSET);
        if (pcb_ptr->status == TASK_BLOCKED)
            do_unblock(ptr);
    }
}