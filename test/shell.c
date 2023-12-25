/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdrtv.h>

#define SHELL_BEGIN 15
#define SHELL_BUFF_LEN 30

#define NAME_MAX_LEN 20
#define ARG_MAX_NUM 20
#define ARG_MAX_LEN 20

char buffer[SHELL_BUFF_LEN];
int buffer_cur = -1;

char arg[ARG_MAX_NUM][ARG_MAX_NUM];
char *argv[ARG_MAX_NUM];
int argc;

/* [p3] small tools :D */
int my_getchar(){
    int ch = sys_getchar();
    while (ch == -1)    ch = sys_getchar();
    return ch;
}

void my_putchar(int ch){
    char tmp_buff[2] = {ch, '\0'};
    sys_write(tmp_buff);
    sys_reflush();
}

void backspace(){
    sys_move_cursor_c(-1, 0);
    sys_write(" ");
    sys_move_cursor_c(-1, 0);
    sys_reflush();
}

/* buffer parser */
int parse_buffer(){
    int len = strlen(buffer);
    argc = 0;
    for (int index = 0, cur = 0; index < len;){
        while (isspace(buffer[index]) && index < len) index++;
        if (index < len){
            cur = 0;
            while (!isspace(buffer[index]) && index < len){
                arg[argc][cur] = buffer[index];
                cur++;
                index++;
            }
            arg[argc][cur] = '\0';
            argc++;
        }
    }
    return argc;
}

/* [p3] check the buffer to get the cmd */
void ps(){
    sys_ps();
}

void ms(){
    sys_ms();
}

void clear(){
    sys_clear();
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------------------- COMMAND -------------------------------\n");
    sys_reflush();
}

void exec(){

    // args
    for (int i = 1; i < argc; i ++){
        argv[i - 1] = arg[i];
    }
    int need_wait = strcmp(arg[argc - 1], "&");

    // syscall
    pid_t new_pid = sys_exec(argv[0], argc - 1, argv);
    if (new_pid == 0){
        printf("[Info] Invalid process name. :(\n");
    }
    else{
        printf("[Info] Successfully load %s, pid = %d. :)\n", argv[0], new_pid);
        if (need_wait)
            sys_waitpid(new_pid);
    }
}

void kill(){
    int buffer_len = strlen(buffer);
    int cur = 4, pid = 0;
    while (buffer[cur] == ' ' && cur < buffer_len) cur++;
    while (buffer[cur] != ' ' && cur < buffer_len){
        pid = pid * 10 + buffer[cur] - '0';
        cur++;
    }
    int rtval = sys_kill(pid);
    switch (rtval) {
        case KILL_FAIL:
            printf("[Info] Fail to kill pid %d. :(\n", pid);
            break;
        case KILL_SUCCESS:
            printf("[Info] Target pid %d is down! :)\n", pid);
            break;
        case KILL_ZOMBIE:
            printf("[Info] Target pid %d is zombied! :D\n", pid);
            printf("[Hint] Children threads are still running.\n");
            break;
        default:
            printf("[Info] Unknown err. (code 0x%x) :(\n", rtval);
            break;
    }
}

void taskset(){
    // check for '-p'
    int form = (strcmp(arg[1], "-p") == 0);
    int need_wait = strcmp(arg[argc - 1], "&");

    // get the mask
    int mask = form ? atoi(arg[2]) : atoi(arg[1]);

    // get the name or pid
    if (form == 1){     // pid
        int pid = atoi(arg[3]);
        int rtval = sys_taskset(0, mask, pid);
        if (rtval)
            printf("[Info] bind pid = %d with mask 0x%x\n", pid, mask);
        else{
            printf("[Info] Fail to bind! Plz check the pid :(\n");
        }
    }
    else{
        int new_pid = sys_taskset(arg[2], mask, 0);
        if (new_pid){
            printf("[Info] start %s with mask 0x%x\n", arg[2], mask);
            if (need_wait)
                sys_waitpid(new_pid);
        }
        else{
            printf("[Info] Fail to start and bind! Plz check the proc name :(\n");
        }
    }
}

void mkfs(){
    int force = (strcmp(arg[1], "-f") == 0);
    sys_mkfs(force);
}

void cd(){
    sys_cd(arg[1]);
}

void ls(){
    int mode = 0;
    char *path = NULL;
    for (int i = 1; i < argc; i++){
        if (!strcmp(arg[i], "-a")){
            mode |= 0b01;
            continue;
        }
        if (!strcmp(arg[i], "-l")){
            mode |= 0b10;
            continue;
        }
        if (!strcmp(arg[i], "-al") || !strcmp(arg[i], "-la")){
            mode |= 0b11;
            continue;
        }
        path = arg[i];
    }
    sys_ls(mode, path);
}

void statfs(){
    sys_statfs();
}

void mkdir(){
    if (argc < 2){
        printf("[FS] Error: missing operend.\n");
    }
    else {
        sys_mkdir(arg[1]);
    }
}

void rmdir(){
    if (argc < 2){
        printf("[FS] Error: missing operand.\n");
    }
    else{
        sys_rmdir(arg[1]);
    }
}

void touch(){
    if (argc < 2){
        printf("[FS] Error: missing operand.\n");
    }
    else{
        sys_touch(arg[1]);
    }
}

void pwd(){
    sys_pwd();
}

void rm(){
    if (argc < 2){
        printf("[FS] Error: missing operand.\n");
    }
    else {
        sys_rm(arg[1]);
    }
}

void cat(){
    if (argc < 2){
        printf("[FS] Error: missing operand.\n");
    }
    else{
        sys_cat(arg[1]);
    }
}

int check_cmd(){
    int cmd_found = 0;
    parse_buffer();
    if (!strcmp(arg[0], "ps")){
        ps();
        return cmd_found = 1;
    }
    if (!strcmp(arg[0], "ms")){
        ms();
        return cmd_found = 1;
    }
    if (!strcmp(arg[0], "clear")){
        clear();
        return cmd_found = 1;
    }
    if (!strcmp(arg[0], "exec")){
        exec();
        return cmd_found = 1;
    }
    if (!strcmp(arg[0], "kill")){
        kill();
        return cmd_found = 1;
    }
    if (!strcmp(arg[0], "taskset")){
        taskset();
        return cmd_found = 1;
    }
    if (!strcmp(arg[0], "mkfs")){
        mkfs();
        return cmd_found = 1;
    }
    if (!strcmp(arg[0], "statfs")){
        statfs();
        return cmd_found = 1;
    }
    if (!strcmp(arg[0], "cd")){
        cd();
        return cmd_found = 1;
    }
    if (!strcmp(arg[0], "ls")){
        ls();
        return cmd_found = 1;
    }
    if (!strcmp(arg[0], "mkdir")){
        mkdir();
        return cmd_found = 1;
    }
    if (!strcmp(arg[0], "pwd")){
        pwd();
        return cmd_found = 1;
    }
    if (!strcmp(arg[0], "rmdir")){
        rmdir();
        return cmd_found = 1;
    }
    if (!strcmp(arg[0], "touch")){
        touch();
        return cmd_found = 1;
    }
    if (!strcmp(arg[0], "rm")){
        rm();
        return cmd_found = 1;
    }
    if (!strcmp(arg[0], "cat")){
        cat();
        return cmd_found = 1;
    }
    return cmd_found;
}

/* [p3] char handler */
int input_handler(int ch){              // 1: need to chk cmd (\r), 0: just continue
    if (ch == '\b' || ch == 127){
        if (buffer_cur >= 0){
            buffer[buffer_cur] = '\0';
            buffer_cur--;
            backspace();
        }
        return 0;
    }
    else{
        if (ch != '\r'){
            buffer_cur++;
            buffer[buffer_cur] = ch;
            my_putchar(ch);
            return 0;
        }
        else{
            buffer_cur++;
            buffer[buffer_cur] = '\0';
            return 1;
        }
    }
}

int main(void)
{
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------------------- COMMAND -------------------------------\n");
    sys_mkfs(1);
    printf("> root@UCAS_OS: ");

    int ch;
    while (1)
    {
        // TODO [P3-task1]: call syscall to read UART port
        ch = my_getchar();

        // TODO [P3-task1]: parse input
        // note: backspace maybe 8('\b') or 127(delete)
        if (!input_handler(ch)) continue;

        // TODO [P3-task1]: ps, exec, kill, clear   
        printf("\n"); 
        if (!check_cmd()){
            printf("[Info] Unknown command :(\n");
        }
        printf("> root@UCAS_OS: ");
        buffer_cur = -1;
        sys_reflush();
        /************************************************************/
        /* Do not touch this comment. Reserved for future projects. */
        /************************************************************/    
    }

    return 0;
}
