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
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define SHELL_BEGIN 20
#define SHELL_BUFF_LEN 30

#define NAME_MAX_LEN 20
#define ARG_MAX_NUM 20
#define ARG_MAX_LEN 20

char buffer[SHELL_BUFF_LEN];
int buffer_cur = -1;

char arg[ARG_MAX_NUM][ARG_MAX_NUM];
char *argv[ARG_MAX_NUM];

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

void move_cur_left(){

}

void move_cur_right(){

}

/* [p3] check the buffer to get the cmd */
void ps(){
    sys_ps();
}

void clear(){
    sys_clear();
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
    sys_reflush();
}

void exec(){
    char name[NAME_MAX_LEN];
    int name_cur, cur, buffer_len;
    name_cur = 0;
    cur = 4;
    buffer_len = strlen(buffer);

    if (buffer[buffer_len - 1] == '&')  buffer_len--;
    
    // get the name !
    while (buffer[cur] == ' ' && cur < buffer_len) cur++;
    while (buffer[cur] != ' ' && cur < buffer_len){
        name[name_cur] = buffer[cur];
        name_cur++;
        cur++;
    }

    // get args !
    int arg_num = 0, tmp_cur = 0;
    cur = 4;
    while (cur < buffer_len){
        while (buffer[cur] == ' ' && cur < buffer_len) cur++;
        tmp_cur = 0;
        while (buffer[cur] != ' ' && cur < buffer_len){
            arg[arg_num][tmp_cur] = buffer[cur];
            argv[arg_num] = (char *)(arg[arg_num]);
            cur++;
            tmp_cur++;
        }
        arg_num++;
    }

    // syscall
    sys_exec(name, arg_num, argv);
}

int check_cmd(){
    int cmd_found = 0;
    if (!strcmp(buffer, "ps")){
        ps();
        cmd_found = 1;
    }
    if (!strcmp(buffer, "clear")){
        clear();
        cmd_found = 1;
    }
    char tmp[5];
    for (int i = 0; i < 4; i++) tmp[i] = buffer[i];
    if (!strcmp(tmp, "exec")){
        exec();
        cmd_found = 1;
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
    printf("------------------- COMMAND -------------------\n");
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
            printf("Unknown command :(\n");
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
