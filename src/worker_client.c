/*****************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/


/* include header */
#include "worker.h"
#include "worker_client.h"
#include "utils.h"
#include "worker_logger.h"


/* callback for task completed */
void *worker_client( ) {

    logger( GM_LOG_TRACE, "worker client started\n" );

    gearman_worker_st worker;
    if(create_gearman_worker(&worker) != GM_OK) {
        logger( GM_LOG_ERROR, "cannot start worker\n" );
        exit( EXIT_FAILURE );
    }

    while ( 1 ) {
        gearman_return_t ret;
        ret = gearman_worker_work( &worker );
        if ( ret != GEARMAN_SUCCESS ) {
            logger( GM_LOG_ERROR, "worker error: %s\n", gearman_worker_error( &worker ) );
            gearman_job_free_all( &worker );
            gearman_worker_free( &worker );
            create_gearman_worker( &worker );
        }
    }

    return NULL;
}

/* get a job */
void *get_job( gearman_job_st *job, void *context, size_t *result_size, gearman_return_t *ret_ptr ) {

    gm_worker_options_t options= *( ( gm_worker_options_t * )context );

    // get the data
    const uint8_t *workload;
    workload= gearman_job_workload( job );
    *result_size= gearman_job_workload_size( job );

    char *result;
    result = malloc( *result_size );
    char *result_c = result;
    if ( result == NULL ) {
        logger( GM_LOG_ERROR, "malloc error\n" );
        *ret_ptr= GEARMAN_WORK_FAIL;
        return NULL;
    }

    snprintf( result, ( int )*result_size, "%.*s", ( int )*result_size, workload );
    logger( GM_LOG_DEBUG, "got result %s%s%s\n", gearman_job_handle( job ),
            options & GM_WORKER_OPTIONS_UNIQUE ? " Unique=" : "",
            options & GM_WORKER_OPTIONS_UNIQUE ? gearman_job_unique( job ) : ""
          );
    logger( GM_LOG_TRACE, "--->\n%.*s\n<---\n", ( int )*result_size, result );

    // set result pointer to success
    *ret_ptr= GEARMAN_SUCCESS;
    if ( options & GM_WORKER_OPTIONS_DATA ) {
        *result_size= 0;
        return NULL;
    }

    char *ptr;
    while ( (ptr = strsep(&result, "\n" )) != NULL ) {
        char *key   = str_token( &ptr, '=' );
        char *value = str_token( &ptr, 0 );

        if ( key == NULL )
            continue;

        if ( value == NULL ) {
            break;
        }

        if ( !strcmp( value, "") ) {
            logger( GM_LOG_ERROR, "got empty value for key %s\n", key );
            continue;
        }

        if ( !strcmp( key, "host_name" ) ) {
        } else if ( !strcmp( key, "service_description" ) ) {
        } else if ( !strcmp( key, "check_options" ) ) {
        } else if ( !strcmp( key, "scheduled_check" ) ) {
        } else if ( !strcmp( key, "reschedule_check" ) ) {
        } else if ( !strcmp( key, "exited_ok" ) ) {
        } else if ( !strcmp( key, "start_time" ) ) {
        } else if ( !strcmp( key, "finish_time" ) ) {
        } else if ( !strcmp( key, "latency" ) ) {
        }
    }
    free(result_c);

    // give gearman something to free
    uint8_t *buffer;
    buffer = malloc( 1 );
    return buffer;
}


/* create the gearman worker */
int create_gearman_worker( gearman_worker_st *worker ) {

    gearman_return_t ret;
    gm_worker_options_t options= GM_WORKER_OPTIONS_NONE;

    if ( gearman_worker_create( worker ) == NULL ) {
        logger( GM_LOG_ERROR, "Memory allocation failure on worker creation\n" );
        return GM_ERROR;
    }

    int x = 0;
    while ( gearman_opt_server[x] != NULL ) {
        char * server   = strdup( gearman_opt_server[x] );
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

    if(gearman_opt_hosts == GM_ENABLED)
        ret = gearman_worker_add_function( worker, "host", 0, get_job, &options );

    if(gearman_opt_services == GM_ENABLED)
        ret = gearman_worker_add_function( worker, "service", 0, get_job, &options );

    if(gearman_opt_events == GM_ENABLED)
        ret = gearman_worker_add_function( worker, "eventhandler", 0, get_job, &options );

    x = 0;
    while ( gearman_hostgroups_list[x] != NULL ) {
        char buffer[8192];
        snprintf( buffer, (sizeof(buffer)-1), "hostgroup_%s", gearman_hostgroups_list[x] );
        ret = gearman_worker_add_function( worker, buffer, 0, get_job, &options );
        x++;
    }

    x = 0;
    while ( gearman_servicegroups_list[x] != NULL ) {
        char buffer[8192];
        snprintf( buffer, (sizeof(buffer)-1), "servicegroup_%s", gearman_servicegroups_list[x] );
        ret = gearman_worker_add_function( worker, buffer, 0, get_job, &options );
        x++;
    }

    if ( ret != GEARMAN_SUCCESS ) {
        logger( GM_LOG_ERROR, "worker error: %s\n", gearman_worker_error( worker ) );
        return GM_ERROR;
    }

    return GM_OK;
}
