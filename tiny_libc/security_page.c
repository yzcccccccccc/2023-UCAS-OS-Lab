#include <security_page.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>

uint64_t secPage_ptr = SECURITY_BASE;
int secPage_mlock_handle = -1;

// [p4-task3]
uint64_t copy_argv_to_secPage(char **argv, int argc){
    if (secPage_mlock_handle == -1){            // init the lock!
        int cur_pid = sys_getpid();
        secPage_mlock_handle = sys_mutex_init(cur_pid + MAGIC_NUM);
    }

    sys_mutex_acquire(secPage_mlock_handle);
    char **rt_argv = (char **)secPage_ptr;
    secPage_ptr += sizeof(uint64_t) * argc;
    for (int i = 0, len; i < argc; i++){
        rt_argv[i] = (char *)secPage_ptr;
        len = strlen(argv[i]) + 1;
        strcpy(rt_argv[i], argv[i]);
        secPage_ptr += len;
    }
    sys_mutex_release(secPage_mlock_handle);

    return (uint64_t)rt_argv;
}

// [p4-task3]
uint64_t copy_str_to_secPage(char *str){
    if (secPage_mlock_handle == -1){            // init the lock!
        int cur_pid = sys_getpid();
        secPage_mlock_handle = sys_mutex_init(cur_pid + MAGIC_NUM);
    }

    sys_mutex_acquire(secPage_mlock_handle);
    char *rt_str = (char *)secPage_ptr;
    strcpy(rt_str, str);
    int len = strlen(str) + 1;
    secPage_ptr += len;
    sys_mutex_release(secPage_mlock_handle);
    
    return (uint64_t)rt_str;
}

// [p4-task3]
void copy_secPage_to_ptr(void *secPage_ptr, void *ptr, int len){
    if (secPage_mlock_handle == -1){            // init the lock!
        int cur_pid = sys_getpid();
        secPage_mlock_handle = sys_mutex_init(cur_pid + MAGIC_NUM);
    }

    sys_mutex_acquire(secPage_mlock_handle);
    for (int i = 0; i < len; i++){
        *(char *)(ptr + i) = *(char *)(secPage_ptr + i);
    }
    sys_mutex_release(secPage_mlock_handle);
}