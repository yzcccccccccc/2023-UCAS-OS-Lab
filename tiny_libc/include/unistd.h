#ifndef __UNISTD_H__
#define __UNISTD_H__

#include <stdint.h>
typedef int32_t pid_t;
typedef pid_t pthread_t;

void sys_sleep(uint32_t time);
void sys_yield(void);
void sys_write(char *buff);
void sys_move_cursor(int x, int y);
void sys_reflush(void);
long sys_get_timebase(void);
long sys_get_tick(void);
int sys_mutex_init(int key);
void sys_mutex_acquire(int mutex_idx);
void sys_mutex_release(int mutex_idx);
void sys_thread_yield(void);
int sys_thread_create(uint64_t entry_addr, void *arg);
int sys_thread_join(int thread_id);

/************************************************************/
/* TODO: [P3 task1] ps, getchar */
void sys_ps(void);
int  sys_getchar(void);
void sys_ms(void);

/* TODO: [P3 task1] exec, exit, kill waitpid */
// S-core
//pid_t  sys_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2);
// A/C-core
 pid_t  sys_exec(char *name, int argc, char **argv);

void sys_exit(void);
int  sys_kill(pid_t pid);
int  sys_waitpid(pid_t pid);
pid_t sys_getpid();

/* [p3] move cursor comparatively */
void sys_move_cursor_c(int dx, int dy);

/* [p3] clear the screen */
void sys_clear();

/* TODO: [P3 task2] barrier */ 
int  sys_barrier_init(int key, int goal);
void sys_barrier_wait(int bar_idx);
void sys_barrier_destroy(int bar_idx);

/* TODO: [P3 task2] condition */ 
int sys_condition_init(int key);
void sys_condition_wait(int cond_idx, int mutex_idx);
void sys_condition_signal(int cond_idx);
void sys_condition_broadcast(int cond_idx);
void sys_condition_destroy(int cond_idx);

/* TODO: [P3 task2] semaphore */
void sys_semaphore_up(int sema_idx);
void sys_semaphore_down(int sema_idx);
int sys_semaphore_init(int key, int init);
void sys_semaphore_destroy(int sema_idx);

/* TODO: [P3 task2] mailbox */ 
int sys_mbox_open(char * name);
void sys_mbox_close(int mbox_id);
int sys_mbox_send(int mbox_idx, void *msg, int msg_length);
int sys_mbox_recv(int mbox_idx, void *msg, int msg_length);

/* TODO: [P4-task5] shmpageget/dt */
/* shmpageget/dt */
void* sys_shmpageget(int key);
void sys_shmpagedt(void *addr);

/* net send and recv */
int sys_net_send(void *txpacket, int length);
int sys_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens);
void sys_net_recv_stream(void *buffer, int *nbytes);
/************************************************************/

/* [p3-task4] taskset */
int sys_taskset(char *name, int mask, int pid);

// [p6]
void sys_mkfs(int force);
void sys_statfs();
int sys_cd(char *path);
int sys_ls(int mode, char *path);
int sys_mkdir(char *path);
int sys_rmdir(char *path);
int sys_fopen(char *name, int option);
int sys_fclose(int fd);
int sys_fread(int fd, char *buff, int length);
int sys_fwrite(int fd, char *buff, int length);
int sys_touch(char *path);
int sys_rm(char *path);
void sys_pwd();
int sys_cat(char *path);

#endif
