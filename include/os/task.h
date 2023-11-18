#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>
#include <os/sched.h>

#define TASK_MEM_BASE    0x52000000
#define TASK_MAXNUM      NUM_MAX_TASK
#define TASK_SIZE        0x10000
#define TASK_NAME_LEN    32


#define SECTOR_SIZE 512
#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

/* TODO: [p1-task4] implement your own task_info_t! */
typedef struct {
    char task_name[TASK_NAME_LEN];
    int offset;                                 // offset from bootblock (addr: 0)
    int size;
} task_info_t;

extern task_info_t tasks[TASK_MAXNUM];
extern short task_num;                            // [p1-task4] global task_num

/* [p3] init operations */
void init_pcb_stack(ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point, pcb_t *pcb, int argc, char *argv[]);
pid_t init_pcb_vname(char *name, int argc, char *argv[]);

#endif