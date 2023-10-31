/*********************************************************************
        For shell command
        Authored by Yzcc, 2023.10.30
**********************************************************************/
#ifndef INCLUDE_CMD_H
#define INCLUDE_CMD_H

#include <os/sched.h>
#include <os/task.h>
#include <type.h>

void do_process_show();
pid_t do_exec(char *name, int argc, char **argv);
pid_t do_getpid();

#endif