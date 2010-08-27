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

int gearman_opt_debug_level;
char * gearman_opt_server[GM_LISTSIZE];

int main (int argc, char **argv);
void parse_arguments(char **argv);
int make_new_child();
