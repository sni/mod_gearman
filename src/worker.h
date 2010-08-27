/*****************************************************************************
 *
 * mod_gearman.c - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include <libgearman/gearman.h>
#include <signal.h>
#include <sys/wait.h>

#define    OK            0
#define    ERROR         1

typedef struct {
    int pid;
} worker;

int gearman_opt_debug_level;

int main (int argc, char **argv);
void parse_arguments(char **argv);
int make_new_child();
void reaper();
void huntsman();
