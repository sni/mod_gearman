/******************************************************************************
 *
 * mod_gearman.c - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <common.h>
#include <libgearman/gearman.h>

#define GM_DEFAULT_MIN_WORKER           1      // minumum number of worker
#define GM_DEFAULT_MAX_WORKER         200      // maximum number of concurrent worker
#define GM_DEFAULT_JOB_MAX_AGE        600      // discard jobs older than that
#define GM_DEFAULT_TIMEOUT             60
#define GM_MAX_JOBS_PER_CLIENT        100


int    mod_gm_opt_debug_level;
char * mod_gm_opt_server[GM_LISTSIZE];
int    mod_gm_opt_hosts;
int    mod_gm_opt_services;
int    mod_gm_opt_events;
int    mod_gm_opt_debug_result;
int    mod_gm_opt_timeout;
int    mod_gm_opt_max_age;
char * mod_gm_hostgroups_list[GM_LISTSIZE];
char * mod_gm_servicegroups_list[GM_LISTSIZE];

int  main (int argc, char **argv);
void parse_arguments(char **argv);
int  make_new_child(void);
void print_usage(void);
int  adjust_number_of_worker(int min, int max, int cur_workers, int cur_jobs);
void check_signal(int sig);
void setup_child_communicator(void);
