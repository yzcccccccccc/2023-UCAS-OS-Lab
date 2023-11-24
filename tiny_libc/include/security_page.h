#ifndef __SECURITY_H__
#define __SECURITY_H__

#include <stdint.h>

// [p4-task3]
#define SECURITY_BASE 0x5000
#define MAGIC_NUM     114514
#define RESET_SECPAGE_PTR (secPage_ptr = SECURITY_BASE)

extern uint64_t secPage_ptr;
extern int secPage_mlock_handle;

extern uint64_t copy_argv_to_secPage(char *argv[], int argc);
extern uint64_t copy_str_to_secPage(char *str);


#endif