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

int mod_gm_con_errors = 0;
struct timeval mod_gm_error_time;

/* create the gearman worker */
int create_worker( char ** server_list, gearman_worker_st *worker ) {
    int x = 0;

    gearman_return_t ret;
    signal(SIGPIPE, SIG_IGN);

    worker = gearman_worker_create( worker );
    if ( worker == NULL ) {
        gm_log( GM_LOG_ERROR, "Memory allocation failure on worker creation\n" );
        return GM_ERROR;
    }

    while ( server_list[x] != NULL ) {
        char * server   = strdup( server_list[x] );
        char * server_c = server;
        char * host     = strsep( &server, ":" );
        char * port_val = strsep( &server, "\x0" );
        in_port_t port  = GM_SERVER_DEFAULT_PORT;
        if(port_val != NULL) {
            port  = ( in_port_t ) atoi( port_val );
        }
        ret = gearman_worker_add_server( worker, host, port );
        if ( ret != GEARMAN_SUCCESS ) {
            gm_log( GM_LOG_ERROR, "worker error: %s\n", gearman_worker_error( worker ) );
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
        gm_log( GM_LOG_ERROR, "worker error: %s\n", gearman_worker_error( worker ) );
        return GM_ERROR;
    }

    return GM_OK;
}


/* create the gearman duplicate client */
int create_client_dup( char ** server_list, gearman_client_st *client ) {
    gearman_return_t ret;
    int x = 0;

    gm_log( GM_LOG_TRACE, "create_client_dup()\n" );

    signal(SIGPIPE, SIG_IGN);

    client = gearman_client_create(client);
    if ( client == NULL ) {
        gm_log( GM_LOG_ERROR, "Memory allocation failure on client creation\n" );
        return GM_ERROR;
    }

    while ( server_list[x] != NULL ) {
        char * server   = strdup( server_list[x] );
        char * server_c = server;
        char * host     = strsep( &server, ":" );
        char * port_val = strsep( &server, "\x0" );
        in_port_t port  = GM_SERVER_DEFAULT_PORT;
        if(port_val != NULL) {
            port  = ( in_port_t ) atoi( port_val );
        }
        ret = gearman_client_add_server( client, host, port );
        if ( ret != GEARMAN_SUCCESS ) {
            gm_log( GM_LOG_ERROR, "client error: %s\n", gearman_client_error( client ) );
            free(server_c);
            return GM_ERROR;
        }
        free(server_c);
        x++;
    }

    current_client_dup = client;

    return GM_OK;
}

/* create the gearman client */
int create_client( char ** server_list, gearman_client_st *client ) {
    gearman_return_t ret;
    int x = 0;

    gm_log( GM_LOG_TRACE, "create_client()\n" );

    signal(SIGPIPE, SIG_IGN);

    client = gearman_client_create(client);
    if ( client == NULL ) {
        gm_log( GM_LOG_ERROR, "Memory allocation failure on client creation\n" );
        return GM_ERROR;
    }

    while ( server_list[x] != NULL ) {
        char * server   = strdup( server_list[x] );
        char * server_c = server;
        char * host     = strsep( &server, ":" );
        char * port_val = strsep( &server, "\x0" );
        in_port_t port  = GM_SERVER_DEFAULT_PORT;
        if(port_val != NULL) {
            port  = ( in_port_t ) atoi( port_val );
        }
        ret = gearman_client_add_server( client, host, port );
        if ( ret != GEARMAN_SUCCESS ) {
            gm_log( GM_LOG_ERROR, "client error: %s\n", gearman_client_error( client ) );
            free(server_c);
            return GM_ERROR;
        }
        free(server_c);
        x++;
    }
    assert(x != 0);


    current_client = client;

    return GM_OK;
}


/* create a task and send it */
int add_job_to_queue( gearman_client_st *client, char ** server_list, char * queue, char * uniq, char * data, int priority, int retries, int transport_mode, int send_now ) {
    gearman_task_st *task = NULL;
    gearman_return_t ret1, ret2;
    char * crypted_data;
    int size;
    struct timeval now;

    signal(SIGPIPE, SIG_IGN);

    gm_log( GM_LOG_TRACE, "add_job_to_queue(%s, %s, %d, %d, %d, %d)\n", queue, uniq, priority, retries, transport_mode, send_now );
    gm_log( GM_LOG_TRACE, "%d --->%s<---\n", strlen(data), data );

    crypted_data = malloc(GM_BUFFERSIZE);
    size = mod_gm_encrypt(&crypted_data, data, transport_mode);
    gm_log( GM_LOG_TRACE, "%d +++>\n%s\n<+++\n", size, crypted_data );

#ifdef GM_DEBUG
    /* verify decrypted string is equal to the original */
    char * test;
    test = malloc(GM_BUFFERSIZE);
    mod_gm_decrypt(&test, crypted_data, transport_mode);
    gm_log( GM_LOG_TRACE, "%d ===>\n%s\n<===\n", size, test );
    if(strcmp(test, data)) {
        gm_log( GM_LOG_ERROR, "%d --->%s<---\n", strlen(data), data );
        gm_log( GM_LOG_ERROR, "%d ===>\n%s\n<===\n", size, test );
        fprintf(stderr, "encrypted string does not match\n");
        exit(EXIT_FAILURE);
    }
#endif

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
        gm_log( GM_LOG_ERROR, "add_job_to_queue() wrong priority: %d\n", priority );
    }

    if(send_now != TRUE)
        return GM_OK;

    ret2 = gearman_client_run_tasks( client );
    gearman_client_task_free_all( client );
    if(   ret1 != GEARMAN_SUCCESS
       || ret2 != GEARMAN_SUCCESS
       || task == NULL
       || ( gearman_client_error(client) != NULL && atof(gearman_version()) == 0.14 )
      ) {

        /* log the error */
        if(retries == 0) {
            gettimeofday(&now,NULL);
            /* only log the first error, otherwise we would fill the log very quickly */
            if( mod_gm_con_errors == 0 ) {
                gettimeofday(&mod_gm_error_time,NULL);
                gm_log( GM_LOG_ERROR, "sending job to gearmand failed: %s\n", gearman_client_error(client) );
            }
            /* or every minute to give an update */
            else if( now.tv_sec >= mod_gm_error_time.tv_sec + 60) {
                gettimeofday(&mod_gm_error_time,NULL);
                gm_log( GM_LOG_ERROR, "sending job to gearmand failed: %s (%i lost jobs so far)\n", gearman_client_error(client), mod_gm_con_errors );
            }
            mod_gm_con_errors++;
        }

        /* recreate client, otherwise gearman sigsegvs */
        gearman_client_free( client );
        create_client( server_list, client );

        /* retry as long as we have retries */
        if(retries > 0) {
            retries--;
            gm_log( GM_LOG_TRACE, "add_job_to_queue() retrying... %d\n", retries );
            return(add_job_to_queue( client, server_list, queue, uniq, data, priority, retries, transport_mode, send_now ));
        }
        /* no more retries... */
        else {
            gm_log( GM_LOG_TRACE, "add_job_to_queue() finished with errors: %d %d\n", ret1, ret2 );
            return GM_ERROR;
        }
    }

    /* reset error counter */
    mod_gm_con_errors = 0;

    gm_log( GM_LOG_TRACE, "add_job_to_queue() finished sucessfully: %d %d\n", ret1, ret2 );
    return GM_OK;
}


void *dummy( gearman_job_st *job, void *context, size_t *result_size, gearman_return_t *ret_ptr ) {

    /* avoid "unused parameter" warning */
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
