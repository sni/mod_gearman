/******************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein - sven.nierlein@consol.de
 *
 * This file is part of mod_gearman.
 *
 *  mod_gearman is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  mod_gearman is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with mod_gearman.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#include "common.h"
#include "gearman.h"
#include "utils.h"


/* create the gearman worker */
int create_worker( char ** server_list, gearman_worker_st *worker ) {

    gearman_return_t ret;
    signal(SIGPIPE, SIG_IGN);

    worker = gearman_worker_create( worker );
    if ( worker == NULL ) {
        logger( GM_LOG_ERROR, "Memory allocation failure on worker creation\n" );
        return GM_ERROR;
    }

    int x = 0;
    while ( server_list[x] != NULL ) {
        char * server   = strdup( server_list[x] );
        char * server_c = server;
        char * host     = str_token( &server, ':' );
        char * port_val = str_token( &server, 0 );
        in_port_t port  = GM_SERVER_DEFAULT_PORT;
        if(port_val != NULL) {
            port  = ( in_port_t ) atoi( port_val );
        }
        ret = gearman_worker_add_server( worker, host, port );
        if ( ret != GEARMAN_SUCCESS ) {
            logger( GM_LOG_ERROR, "worker error: %s\n", gearman_worker_error( worker ) );
            free(server_c);
            return GM_ERROR;
        }
        free(server_c);
        x++;
    }
    assert(x != 0);

    return GM_OK;
}


/* register function on worker */
int worker_add_function( gearman_worker_st * worker, char * queue, gearman_worker_fn *function) {
    gearman_return_t ret;
    ret = gearman_worker_add_function( worker, queue, 0, function, NULL );
    if ( ret != GEARMAN_SUCCESS ) {
        logger( GM_LOG_ERROR, "worker error: %s\n", gearman_worker_error( worker ) );
        return GM_ERROR;
    }

    return GM_OK;
}


/* create the gearman client */
int create_client( char ** server_list, gearman_client_st *client ) {
    logger( GM_LOG_TRACE, "create_gearman_client()\n" );

    gearman_return_t ret;
    signal(SIGPIPE, SIG_IGN);

    client = gearman_client_create(client);
    if ( client == NULL ) {
        logger( GM_LOG_ERROR, "Memory allocation failure on client creation\n" );
        return GM_ERROR;
    }

    int x = 0;
    while ( server_list[x] != NULL ) {
        char * server   = strdup( server_list[x] );
        char * server_c = server;
        char * host     = str_token( &server, ':' );
        char * port_val = str_token( &server, 0 );
        in_port_t port  = GM_SERVER_DEFAULT_PORT;
        if(port_val != NULL) {
            port  = ( in_port_t ) atoi( port_val );
        }
        ret = gearman_client_add_server( client, host, port );
        if ( ret != GEARMAN_SUCCESS ) {
            logger( GM_LOG_ERROR, "client error: %s\n", gearman_client_error( client ) );
            free(server_c);
            return GM_ERROR;
        }
        free(server_c);
        x++;
    }
    assert(x != 0);

    return GM_OK;
}


/* create a task and send it */
int add_job_to_queue( gearman_client_st *client, char ** server_list, char * queue, char * uniq, char * data, int priority, int retries, int transport_mode ) {
    gearman_task_st *task = NULL;
    gearman_return_t ret1, ret2;

    signal(SIGPIPE, SIG_IGN);

    logger( GM_LOG_TRACE, "add_job_to_queue(%s, %s, %d, %d, %d)\n", queue, uniq, priority, retries, transport_mode );
    logger( GM_LOG_TRACE, "%d --->%s<---\n", strlen(data), data );

    char * crypted_data = malloc(GM_BUFFERSIZE);
    int size = mod_gm_encrypt(&crypted_data, data, transport_mode);
    logger( GM_LOG_TRACE, "%d +++>\n%s\n<+++\n", size, crypted_data );

    if( priority == GM_JOB_PRIO_LOW ) {
        task = gearman_client_add_task_low_background( client, NULL, NULL, queue, uniq, ( void * )crypted_data, ( size_t )size, &ret1 );
        gearman_task_give_workload(task,crypted_data,size);
    }
    else if( priority == GM_JOB_PRIO_NORMAL ) {
        task = gearman_client_add_task_background( client, NULL, NULL, queue, uniq, ( void * )crypted_data, ( size_t )size, &ret1 );
        gearman_task_give_workload(task,crypted_data,size);
    }
    else if( priority == GM_JOB_PRIO_HIGH ) {
        task = gearman_client_add_task_high_background( client, NULL, NULL, queue, uniq, ( void * )crypted_data, ( size_t )size, &ret1 );
        gearman_task_give_workload(task,crypted_data,size);
    }
    else {
        logger( GM_LOG_ERROR, "add_job_to_queue() wrong priority: %d\n", priority );
    }

    ret2 = gearman_client_run_tasks( client );
    gearman_client_task_free_all( client );
    if(   ret1 != GEARMAN_SUCCESS
       || ret2 != GEARMAN_SUCCESS
       || task == NULL
       || (gearman_client_error(client) != NULL && strcmp(gearman_client_error(client), "") != 0)) { // errno is somehow empty, use error instead

        if(retries == 0)
            logger( GM_LOG_ERROR, "add_job_to_queue() failed: %s\n", gearman_client_error(client) );

        // recreate client, otherwise gearman sigsegvs
        gearman_client_free( client );
        create_client( server_list, client );

        // retry as long as we have retries
        if(retries > 0) {
            retries--;
            logger( GM_LOG_TRACE, "add_job_to_queue() retrying... %d\n", retries );
            return(add_job_to_queue( client, server_list, queue, uniq, data, priority, retries, transport_mode));
        }
        // no more retries...
        else {
            logger( GM_LOG_TRACE, "add_job_to_queue() finished with errors: %d %d\n", ret1, ret2 );
            return GM_ERROR;
        }
    }
    logger( GM_LOG_TRACE, "add_job_to_queue() finished sucessfully: %d %d\n", ret1, ret2 );
    return GM_OK;
}


void *dummy( gearman_job_st *job, void *context, size_t *result_size, gearman_return_t *ret_ptr ) {

    // avoid "unused parameter" warning
    job         = job;
    context     = context;
    result_size = 0;
    ret_ptr     = ret_ptr;

    return NULL;
}


/* free client structure */
void free_client(gearman_client_st *client) {
    gearman_client_free( client );
    return;
}


/* free worker structure */
void free_worker(gearman_worker_st *worker) {
    gearman_worker_free( worker );
    return;
}
