#include <security_page.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>
#include <stdio.h>

uint64_t secPage_ptr = SECURITY_BASE;
int secPage_mlock_handle = -1;

// [p4-task3]
uint64_t malloc_secPage(int size){
    int blocknum = BLOCK_OCC_CAL(size) + 1, start_id = -1;

    int mlock_req = (secPage_mlock_handle != -1);

retry:
    if (mlock_req)  sys_mutex_acquire(secPage_mlock_handle);
    for (int i = 0, cnt; i < BLOCK_NUM; i++){
        start_id = -1;
        cnt = 0;
        while (!secPage_occupation[i + cnt] && cnt < blocknum && i + cnt < BLOCK_NUM) cnt++;
        if (cnt == blocknum){
            start_id = i;
            break;
        }
    }
    // fail to alloc consecutively
    if (start_id == -1){
        if (mlock_req)  sys_mutex_release(secPage_mlock_handle);
        goto retry;
    }

    // allocate!
    uint64_t rt_addr = SECURITY_BASE + start_id * BLOCK_SIZE;
    for (int i = start_id; i < start_id + blocknum - 1; i++)
        secPage_occupation[i] = SECPAGE_USED;
    if (start_id + blocknum - 1 < BLOCK_NUM)
        secPage_occupation[start_id + blocknum - 1] = SECPAGE_EOS;

    if (mlock_req)  sys_mutex_release(secPage_mlock_handle);
    return rt_addr;
}

// [p4-task3]
uint64_t copy_argv_to_secPage(char **argv, int argc){
    int bytes = argc * sizeof(uint64_t);

    // get the length
    for (int i = 0; i < argc; i++){
        argv_len[i] = strlen(argv[i]) + 1;
        bytes += argv_len[i];
    }

    uint64_t base_ptr = malloc_secPage(bytes);

    char **rt_argv = (char **)base_ptr;
    base_ptr += argc * sizeof(uint64_t);
    for (int i = 0; i < argc; i++){
        rt_argv[i] = (char *)base_ptr;
        base_ptr += argv_len[i];
        strcpy(rt_argv[i], argv[i]);
    }
    return (uint64_t)rt_argv;
}

// [p4-task3]
uint64_t copy_str_to_secPage(char *str){
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

// [p4-task3]
void free_secPage(uint64_t ptr){
    int mlock_req = (secPage_mlock_handle != -1);
    if (mlock_req)  sys_mutex_acquire(secPage_mlock_handle);

    int start_id = (ptr - SECURITY_BASE) / BLOCK_SIZE, idx;
    for (idx = start_id; secPage_occupation[idx] != SECPAGE_EOS && idx < BLOCK_NUM; idx++)
        secPage_occupation[idx] = SECPAGE_UNUSED;
    if (idx < BLOCK_NUM)
        secPage_occupation[idx] = SECPAGE_UNUSED;

    if (mlock_req)  sys_mutex_release(secPage_mlock_handle);
}

void init_secPage_mlock(){
    if (secPage_mlock_handle != -1) 
        return;
    int cur_pid = sys_getpid();
    secPage_mlock_handle = sys_mutex_init(cur_pid + MAGIC_NUM);
}