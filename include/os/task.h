#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>

#define TASK_MEM_BASE    0x52000000
#define TASK_MAXNUM      16
#define TASK_SIZE        0x10000
#define TASK_NAME_LEN 32


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

#endif