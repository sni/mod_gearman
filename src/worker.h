/******************************************************************************
 *
 * mod_gearman.c - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#define MOD_GM_WORKER

#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <libgearman/gearman.h>
#include "common.h"

int mod_gm_shm_key;
mod_gm_opt_t *mod_gm_opt;

int main (int argc, char **argv);
int store_original_comandline(int argc, char **argv);
int parse_arguments(int argc, char **argv);
int make_new_child(void);
void print_usage(void);
int  adjust_number_of_worker(int min, int max, int cur_workers, int cur_jobs);
void check_signal(int sig);
void setup_child_communicator(void);
void clean_exit(int signal);
int write_pid_file(void);
int verify_options(mod_gm_opt_t *opt);
void reload_config(int sig);
void stop_childs(int mode);
