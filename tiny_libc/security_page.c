#include <security_page.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>
#include <stddef.h>

uint64_t secPage_ptr = SECURITY_BASE;
int secPage_mlock_handle = -1;

// [p4-task3]
uint64_t malloc_secPage(int size){
    int mlock_req = (secPage_mlock_handle != -1);
    if (mlock_req)  sys_mutex_acquire(secPage_mlock_handle);
    // critical section
    if (secPage_ptr + size > SECURITY_BOUND)
        RESET_SECPAGE_PTR;
    uint64_t rt_addr = secPage_ptr;
    secPage_ptr += size;
    if (mlock_req)  sys_mutex_release(secPage_mlock_handle);
    return rt_addr;
}

// [p4-task3]
uint64_t copy_argv_to_secPage(char **argv, int argc){
    if (argv == NULL)
        return (uint64_t)NULL;
    char **rt_argv = (char **)malloc_secPage(argc * sizeof(uint64_t));
    for (int i = 0, len; i < argc; i++){
        if (argv[i] == NULL){
            rt_argv[i] = NULL;
        }
        else{
            len = strlen(argv[i]) + 1;
            rt_argv[i] = (char *)malloc_secPage(len);
            strcpy(rt_argv[i], argv[i]);
        }
    }
    return (uint64_t)rt_argv;
}

// [p4-task3]
uint64_t copy_str_to_secPage(char *str){
    if (str == NULL)
        return (uint64_t)NULL;
    int len = strlen(str) + 1;
    char *rt_str = (char *)malloc_secPage(len);
    strcpy(rt_str, str);
    return (uint64_t)rt_str;
}

// [p4-task3]
void copy_secPage_to_ptr(void *secPage_ptr, void *ptr, int len){
    for (int i = 0; i < len; i++){
        *(char *)(ptr + i) = *(char *)(secPage_ptr + i);
    }
}

// [p4-task3]
void copy_ptr_to_secPage(void *secPage_ptr, void *ptr, int len){
    for (int i = 0; i < len; i++){
        *(char *)(secPage_ptr + i) = *(char *)(ptr + i);
    }
}

void init_secPage_mlock(){
    if (secPage_mlock_handle != -1) 
        return;
    int cur_pid = sys_getpid();
    secPage_mlock_handle = sys_mutex_init(cur_pid + MAGIC_NUM);
}