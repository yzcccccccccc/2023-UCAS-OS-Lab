#ifndef __SECURITY_H__
#define __SECURITY_H__

#include <stdint.h>

// [p4-task3]
#define SECURITY_BASE   0x5000
#define SECURITY_BOUND  0x5f00
#define MAGIC_NUM     114514
#define RESET_SECPAGE_PTR (secPage_ptr = SECURITY_BASE)

#define BLOCK_SIZE              64
#define BLOCK_NUM               60
#define BLOCK_OCC_CAL(bytes)    (((bytes) / BLOCK_SIZE) + ((bytes) % BLOCK_SIZE != 0))

#define SECPAGE_UNUSED  0
#define SECPAGE_USED    1
#define SECPAGE_EOS     2       // end of segment
int secPage_occupation[BLOCK_NUM];

#define ARGV_MAX_NUM    20
int argv_len[ARGV_MAX_NUM];

extern uint64_t secPage_ptr;
extern int secPage_mlock_handle;

extern uint64_t malloc_secPage(int size);
extern void free_secPage(uint64_t ptr);
extern uint64_t copy_argv_to_secPage(char *argv[], int argc);
extern uint64_t copy_str_to_secPage(char *str);
extern void copy_secPage_to_ptr(void *secPage_ptr, void *ptr, int len);
extern void copy_ptr_to_secPage(void *secPage_ptr, void *ptr, int len);
extern void init_secPage_mlock();


#endif