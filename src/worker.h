/*****************************************************************************
 *
 * mod_gearman.c - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include <libgearman/gearman.h>
#include <sys/wait.h>

#define LISTSIZE       512
#define    OK            0
#define    ERROR         1

int gearman_opt_debug_level;
char * gearman_opt_server[LISTSIZE];

int main (int argc, char **argv);
void parse_arguments(char **argv);
int make_new_child();
