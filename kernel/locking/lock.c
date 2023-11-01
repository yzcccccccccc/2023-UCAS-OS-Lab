#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for (int i = 0; i < LOCK_NUM; i++){
        spin_lock_init(&mlocks[i].lock);
        mlocks[i].block_queue.next = mlocks[i].block_queue.prev = &mlocks[i].block_queue;
        mlocks[i].key = -1;
    }
}

void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
    atomic_swap(UNLOCKED, (ptr_t)(&lock->status));
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    return atomic_swap(LOCKED, (ptr_t)(&lock->status));
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
    while (spin_lock_try_acquire(lock) == LOCKED);
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
    atomic_swap(UNLOCKED, (ptr_t)(&lock->status));
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    int lock_id = -1;
    for (int i = 0; i < LOCK_NUM; i++){
        if (mlocks[i].key == key){
            lock_id = i;
            break;
        }
    }
    if (lock_id == -1){                         // allocate one
        for (int i = 0; i < LOCK_NUM; i++){
            if (mlocks[i].key == -1){
                lock_id = i;
                mlocks[i].key = key;
                mlocks[i].pid = -1;
                break;
            }
        }
    }
    return lock_id;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    if (spin_lock_try_acquire(&(mlocks[mlock_idx].lock)) == LOCKED){
        do_block(&(current_running->list), &(mlocks[mlock_idx].block_queue));
    }
    else{
        mlocks[mlock_idx].pid = current_running->pid;
    }
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
retry:
    if (list_empty(&(mlocks[mlock_idx].block_queue))){
        spin_lock_release(&(mlocks[mlock_idx].lock));
        mlocks[mlock_idx].pid = -1;
    }
    else{
        list_node_t *list_ptr;
        pcb_t *pcb_ptr;

        list_ptr = list_pop(&(mlocks[mlock_idx].block_queue));
        pcb_ptr = (pcb_t *)((void *)list_ptr - LIST_PCB_OFFSET);
        if (pcb_ptr->status == TASK_EXITED)
            goto retry;

        mlocks[mlock_idx].pid = pcb_ptr->pid;
        do_unblock(list_ptr);
    }
}

/* [p3] release lock resources */
void lock_resource_release(pid_t pid){
    /*****************************************
        Hint:
            only handle the acquired locks.
            block_queue situation will be 
        handled when release a mutex.
    ******************************************/
    for (int i = 0; i < LOCK_NUM; i++){
        if (mlocks[i].pid == pid){
            do_mutex_lock_release(i);
        }
    }
}