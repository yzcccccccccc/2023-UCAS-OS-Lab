/**************************************
    Large File Test
    Authored by yzcc, 2023.12.26
***************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define _8KB_OFFSET 8192
#define _8MB_OFFSET 8388608

#define _4MB 4194304

const int print_location = 5;

int main(int argc, char *argv[]){
    int fd = sys_fopen("genshin", O_RDWR);

    if (fd == -1){
        printf("[lf-test] File doesn't exist, exiting...\n");
        return 0;
    }

    if (argc < 2){
        printf("[lf-test] Insufficient args, exiting...\n");
        return 0;
    }

    sys_move_cursor(0, print_location);
    if (!strcmp(argv[1], "SEEK_SET")){
        if (sys_lseek(fd, _8KB_OFFSET, SEEK_SET, SEEK_RP) != -1){
            printf("[lf-test] lseek-set 8KB pass.\n");
        }
        else {
            printf("[lf-test] lseek-set 8KB fail.\n");
        }
        if (sys_lseek(fd, _8MB_OFFSET, SEEK_SET, SEEK_RP) != -1){
            printf("[lf-test] lseek-set 8MB pass.\n");
        }
        else{
            printf("[lf-test] lseek-set 8MB fail.\n");
        }
    }
    if (!strcmp(argv[1], "SEEK_CUR")){
        if (sys_lseek(fd, _4MB, SEEK_CUR, SEEK_RP) != -1){
            printf("[lf-test] lseek-cur 4MB pass.\n");
        }
        else{
            printf("[lf-test] lseek-cur 4MB fail.\n");
        }
    }
    if (!strcmp(argv[1], "SEEK_END")){
        if (sys_lseek(fd, _4MB, SEEK_END, SEEK_RP) != -1){
            printf("[lf-test] lseek-end 4MB pass.\n");
        }
        else{
            printf("[lf-test] lseek-end 4MB fail.\n");
        }
    }
    sys_fclose(fd);
    return 0;
}