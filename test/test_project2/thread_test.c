#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define BUFF_LEN        100
#define MAX_THREAD      10
#define MAX_LIMIT       5
#define LOCATION_BASE   6

/****************************************************
    shared_buff[2]: yield_time
    shared_buff[0]: thread1 iteration
    shared_buff[1]: thread2 iteration
*****************************************************/
int shared_buff[BUFF_LEN];

/*****************************************************
    For Peterson Algorithm.
*****************************************************/
bool thread_wants[MAX_THREAD][BUFF_LEN];
int turn[BUFF_LEN];

void user_acquire(uint64_t arg, int idx){
    thread_wants[arg][idx] = true;
    turn[idx] = arg ^ 1;
    while (thread_wants[arg ^ 1][idx] && turn[idx] == (arg ^ 1));
    return;
}

void user_release(uint64_t arg, int idx){
    thread_wants[arg][idx] = false;
}

void thread_work(uint64_t arg){
    int print_location = LOCATION_BASE + arg + 1;
    while (1){
        if (shared_buff[arg ^ 1] + MAX_LIMIT < shared_buff[arg]){

            sys_move_cursor(0, print_location);
            printf("> [TASK: thread] Sub-thread %ld step %d, yielding...\n", arg, shared_buff[arg]);

            user_acquire(arg, 2);
            shared_buff[2]++;       // yield time
            user_release(arg, 2);

            sys_thread_yield();
        }
        else{

            sys_move_cursor(0, print_location);
            printf("> [TASK: thread] Sub-thread %ld step %d, running...\n", arg, shared_buff[arg]);
            
            user_acquire(arg, arg);
            shared_buff[arg]++;
            user_release(arg, arg);
        }
    }
}

int main(){
    sys_thread_create((uint64_t)thread_work, 0);
    sys_thread_create((uint64_t)thread_work, 1);
    int print_location = LOCATION_BASE;
    while (1){

        sys_move_cursor(0, print_location);
        printf("> [TASK: thread] Main-thread running... Yield Time: %d\n", shared_buff[2]);
    }
    return 0;
}