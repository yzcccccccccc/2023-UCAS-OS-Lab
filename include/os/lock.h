/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                                   Thread Lock
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#ifndef INCLUDE_LOCK_H_
#define INCLUDE_LOCK_H_

#include <os/list.h>
#include <os/sched.h>

#define LOCK_NUM 16

typedef enum {
    UNLOCKED,
    LOCKED,
} lock_status_t;

typedef struct spin_lock
{
    volatile lock_status_t status;
} spin_lock_t;

typedef struct mutex_lock
{
    spin_lock_t lock;
    list_head block_queue;
    int key;
    pid_t pid;                  // [p3] owner's pid, init: -1
} mutex_lock_t;

extern mutex_lock_t mlocks[LOCK_NUM];

void init_locks(void);

void spin_lock_init(spin_lock_t *lock);
int spin_lock_try_acquire(spin_lock_t *lock);
void spin_lock_acquire(spin_lock_t *lock);
void spin_lock_release(spin_lock_t *lock);

int do_mutex_lock_init(int key);
void do_mutex_lock_acquire(int mlock_idx);
void do_mutex_lock_release(int mlock_idx);

void lock_resource_release(pcb_t *pcb_ptr);
void do_mlock_init_ptr(mutex_lock_t *mlock_ptr);
void do_mlock_release_ptr(mutex_lock_t *mlock_ptr);
void do_mlock_acquire_ptr(mutex_lock_t *mlock_ptr);

/************************************************************/
typedef struct barrier
{
    // TODO [P3-TASK2 barrier]
    int goal;               // goal waiting num
    int cur_num;            // current waiting num
    int key;                // handle
    list_head wait_queue;
} barrier_t;

#define BARRIER_NUM 16

void init_barriers(void);
int do_barrier_init(int key, int goal);
void do_barrier_wait(int bar_idx);
void do_barrier_destroy(int bar_idx);

typedef struct condition
{
    // TODO [P3-TASK2 condition]
    int key;
    list_head wait_queue;
} condition_t;

#define CONDITION_NUM 16

void init_conditions(void);
int do_condition_init(int key);
void do_condition_wait(int cond_idx, int mutex_idx);
void do_condition_signal(int cond_idx);
void do_condition_broadcast(int cond_idx);
void do_condition_destroy(int cond_idx);

void do_cond_init_ptr(condition_t *cond_ptr);
void do_cond_wait_ptr(condition_t *cond_ptr, mutex_lock_t *mlock_ptr);
void do_cond_signal_ptr(condition_t *cond_ptr);
void do_cond_broadcast_ptr(condition_t *cond_ptr);

typedef struct semaphore
{
    // TODO [P3-TASK2 semaphore]
    // spin_lock_t spin_lock; (maybe of use when facing fine lock :D)
    int num;                // num of resources
    int key;
    list_head wait_queue;
} semaphore_t;

#define SEMAPHORE_NUM 16

void init_semaphores(void);
int do_semaphore_init(int key, int init);
void do_semaphore_up(int sema_idx);
void do_semaphore_down(int sema_idx);
void do_semaphore_destroy(int sema_idx);

#define MAX_MBOX_LENGTH (16)
#define MAX_MBOX_NAME_LEN (20)

typedef struct mailbox
{
    // TODO [P3-TASK2 mailbox]
    char name[MAX_MBOX_NAME_LEN];
    char msg[MAX_MBOX_LENGTH];
    int allocated;              // 0 for unallocated
    int head, tail, freespace;
    mutex_lock_t mlock;
    condition_t is_full, is_empty;
} mailbox_t;

#define MBOX_NUM 16
void init_mbox();
int do_mbox_open(char *name);
void do_mbox_close(int mbox_idx);
int do_mbox_send(int mbox_idx, void * msg, int msg_length);
int do_mbox_recv(int mbox_idx, void * msg, int msg_length);

/************************************************************/

#define MLOCK_MAGIC_NUM 114514

#endif
