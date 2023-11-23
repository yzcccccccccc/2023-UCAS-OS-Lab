#include <security_page.h>
#include <string.h>

uint64_t secPage_ptr = SECURITY_BASE;

// [p4-task3]
uint64_t copy_argv_to_secPage(char **argv, int argc){
    char **rt_argv = (char **)secPage_ptr;
    secPage_ptr += sizeof(uint64_t) * argc;
    for (int i = 0, len; i < argc; i++){
        rt_argv[i] = (char *)secPage_ptr;
        len = strlen(argv[i]) + 1;
        strcpy(rt_argv[i], argv[i]);
        secPage_ptr += len;
    }
    return (uint64_t)rt_argv;
}

// [p4-task3]
uint64_t copy_str_to_secPage(char *str){
    char *rt_str = (char *)secPage_ptr;
    strcpy(rt_str, str);
    int len = strlen(str) + 1;
    secPage_ptr += len;
    return (uint64_t)rt_str;
}