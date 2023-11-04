#include <os/list.h>
#include <os/sched.h>
#include <type.h>

uint64_t time_elapsed = 0;
uint64_t time_base = 0;

uint64_t get_ticks()
{
    __asm__ __volatile__(
        "rdtime %0"
        : "=r"(time_elapsed));
    return time_elapsed;
}

uint64_t get_timer()
{
    return get_ticks() / time_base;
}

uint64_t get_time_base()
{
    return time_base;
}

void latency(uint64_t time)
{
    uint64_t begin_time = get_timer();

    while (get_timer() - begin_time < time);
    return;
}

void check_sleeping(void)
{
    // TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
    list_node_t *head = &sleep_queue, *ptr, *n_ptr;
    pcb_t *n_pcb;
    uint64_t now_time;

    ptr = head->next;
    while (!list_empty(head) && ptr != head){
        n_ptr = ptr->next;
        n_pcb = (pcb_t *)((void *)ptr - LIST_PCB_OFFSET);
        now_time = get_timer();
        if (now_time >= n_pcb->wakeup_time && n_pcb->status == TASK_BLOCKED){
            n_pcb->status = TASK_READY;
            list_delete(ptr);
            list_insert(&ready_queue, ptr);
        }
        ptr = n_ptr;
    }
    
}