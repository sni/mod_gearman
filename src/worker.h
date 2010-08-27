/*****************************************************************************
 *
 * mod_gearman.c - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include <common.h>
#include <libgearman/gearman.h>
#include <sys/wait.h>

#define GM_DEFAULT_MIN_WORKER           3
#define GM_DEFAULT_MAX_WORKER          50
#define GM_DEFAULT_JOB_MAX_AGE        120


int gearman_opt_debug_level;
char * gearman_opt_server[GM_LISTSIZE];
int gearman_opt_hosts;
int gearman_opt_services;
int gearman_opt_events;
int gearman_opt_debug_result;
char *gearman_hostgroups_list[GM_LISTSIZE];
char *gearman_servicegroups_list[GM_LISTSIZE];

int main (int argc, char **argv);
void parse_arguments(char **argv);
int make_new_child();
void print_usage();
