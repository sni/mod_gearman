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
#include <assert.h>
#include <signal.h>
#include <libgearman/gearman.h>

typedef void*( mod_gm_worker_fn)(gearman_job_st *job, void *context, size_t *result_size, gearman_return_t *ret_ptr);

int create_client( char ** server_list, gearman_client_st * client);
int create_worker( char ** server_list, gearman_worker_st * worker);
int add_job_to_queue( gearman_client_st *client, char ** server_list, char * queue, char * uniq, char * data, int priority, int retries, int transport_mode );
int worker_add_function( gearman_worker_st * worker, char * queue, gearman_worker_fn *function);
void *dummy( gearman_job_st *, void *, size_t *, gearman_return_t * );
void free_client(gearman_client_st *client);
void free_worker(gearman_worker_st *worker);