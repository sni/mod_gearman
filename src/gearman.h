/******************************************************************************
 *
 * mod_gearman.c - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libgearman/gearman.h>

int create_gearman_client( char ** server_list, gearman_client_st *client );
int add_job_to_queue( gearman_client_st *client, char * queue, char * uniq, char * data, int priority, int retries );
