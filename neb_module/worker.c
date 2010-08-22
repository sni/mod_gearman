/*****************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/


/* include header */
#include "worker.h"
#include "utils.h"
#include "mod_gearman.h"
#include "logger.h"

/* callback for task completed */
void *result_worker( void * args ) {

    gearman_return_t ret;
    gearman_worker_st worker;
    gm_worker_options_t options= GM_WORKER_OPTIONS_NONE;

    if ( gearman_worker_create( &worker ) == NULL ) {
        logger( GM_ERROR, "Memory allocation failure on worker creation\n" );
        return;
    }

    int x = 0;
    while ( gearman_opt_server[x] != NULL ) {
        char * server  = strdup( gearman_opt_server[x] );
        x++;
        if ( strchr( server, ':' ) == NULL ) {
            break;
        };
        char * host    = str_token( &server, ':' );
        in_port_t port = ( in_port_t ) atoi( str_token( &server, 0 ) );
        ret=gearman_worker_add_server( &worker, host, port );
        if ( ret != GEARMAN_SUCCESS ) {
            logger( GM_ERROR, "%s\n", gearman_worker_error( &worker ) );
            return;
        }
        logger( GM_DEBUG, "worker added gearman server %s:%i\n", host, port );
    }

    logger( GM_DEBUG, "started result_worker thread for queue: %s\n", gearman_opt_result_queue );

    if ( gearman_opt_result_queue == NULL ) {
        logger( GM_ERROR, "got no result queue!\n" );
        exit( 1 );
    }

    ret = gearman_worker_add_function( &worker, gearman_opt_result_queue, 0, get_results, &options );
    gearman_worker_add_function( &worker, "blah", 0, get_results, NULL ); // somehow the last function is ignored, so in order to set the first one active. Add a useless one
    if ( ret != GEARMAN_SUCCESS ) {
        logger( GM_ERROR, "worker error: %s\n", gearman_worker_error( &worker ) );
        return;
    }

    while ( 1 ) {
        ret = gearman_worker_work( &worker );
        if ( ret != GEARMAN_SUCCESS ) {
            logger( GM_ERROR, "worker error: %s\n", gearman_worker_error( &worker ) );
            //break;
        }
        gearman_job_free_all( &worker );
    }

    gearman_worker_free( &worker );

    return;
}

/* put back the result into the core */
void *get_results( gearman_job_st *job, void *context, size_t *result_size, gearman_return_t *ret_ptr ) {

    gm_worker_options_t options= *( ( gm_worker_options_t * )context );
    const uint8_t *workload;
    char *result;

    // get the data
    workload= gearman_job_workload( job );
    *result_size= gearman_job_workload_size( job );

    result = malloc( *result_size );
    if ( result == NULL ) {
        fprintf( stderr, "malloc:%d\n", errno );
        *ret_ptr= GEARMAN_WORK_FAIL;
        return NULL;
    }

    snprintf( result, ( int )*result_size, "%.*s", ( int )*result_size, workload );
    logger( GM_DEBUG, "got result %s%s%s\n", gearman_job_handle( job ),
            options & GM_WORKER_OPTIONS_UNIQUE ? " Unique=" : "",
            options & GM_WORKER_OPTIONS_UNIQUE ? gearman_job_unique( job ) : ""
          );
    logger( GM_TRACE, "--->\n%.*s\n<---\n", ( int )*result_size, result );

    // set result pointer to success
    *ret_ptr= GEARMAN_SUCCESS;
    if ( options & GM_WORKER_OPTIONS_DATA ) {
        *result_size= 0;
        return NULL;
    }

    // nagios will free it after processing
    check_result * chk_result;
    if ( ( chk_result = ( check_result * )malloc( sizeof *chk_result ) ) == 0 ) {
        *ret_ptr= GEARMAN_WORK_FAIL;
        return NULL;
    }

    chk_result->service_description = NULL;
    chk_result->host_name           = NULL;
    chk_result->output              = NULL;

    char *ptr;
    while ( ptr = strsep( &result, "\n" ) ) {
        char *key   = str_token( &ptr, '=' );
        char *value = str_token( &ptr, 0 );

        if ( key == NULL )
            continue;

        if ( value == NULL ) {
            break;
        }

        if ( value == "" ) {
            logger( GM_ERROR, "got empty value for key %s\n", key );
            continue;
        }

        if ( !strcmp( key, "host_name" ) ) {
            chk_result->host_name = strdup( value );
        } else if ( !strcmp( key, "service_description" ) ) {
            chk_result->service_description = strdup( value );
        } else if ( !strcmp( key, "check_options" ) ) {
            chk_result->check_options = atoi( value );
        } else if ( !strcmp( key, "scheduled_check" ) ) {
            chk_result->scheduled_check = atoi( value );
        } else if ( !strcmp( key, "reschedule_check" ) ) {
            chk_result->reschedule_check = atoi( value );
        } else if ( !strcmp( key, "exited_ok" ) ) {
            chk_result->exited_ok = atoi( value );
        } else if ( !strcmp( key, "early_timeout" ) ) {
            chk_result->early_timeout = atoi( value );
        } else if ( !strcmp( key, "return_code" ) ) {
            chk_result->return_code = atoi( value );
        } else if ( !strcmp( key, "output" ) ) {
            chk_result->output = strdup( value );
        } else if ( !strcmp( key, "start_time" ) ) {
            int sec   = atoi( str_token( &value, '.' ) );
            int usec  = atoi( str_token( &value, 0 ) );
            chk_result->start_time.tv_sec    = sec;
            chk_result->start_time.tv_usec   = usec;
        } else if ( !strcmp( key, "finish_time" ) ) {
            int sec   = atoi( str_token( &value, '.' ) );
            int usec  = atoi( str_token( &value, 0 ) );
            chk_result->finish_time.tv_sec    = sec;
            chk_result->finish_time.tv_usec   = usec;
        } else if ( !strcmp( key, "latency" ) ) {
            chk_result->latency = atof( value );
        }
    }

    if ( chk_result == NULL ) {
        *ret_ptr= GEARMAN_WORK_FAIL;
        return NULL;
    }

    if ( chk_result->host_name == NULL || chk_result->output == NULL ) {
        *ret_ptr= GEARMAN_WORK_FAIL;
        return NULL;
    }

    if ( chk_result->service_description != NULL ) {
        chk_result->object_check_type    = SERVICE_CHECK;
        chk_result->check_type           = SERVICE_CHECK_ACTIVE;
    } else {
        chk_result->object_check_type    = HOST_CHECK;
        chk_result->check_type           = HOST_CHECK_ACTIVE;
    }

    // initialize and fill with result info
    chk_result->output_file    = 0;
    chk_result->output_file_fd = -1;

    if ( chk_result->service_description != NULL ) {
        logger( GM_DEBUG, "service job completed: %s %s: %d\n", chk_result->host_name, chk_result->service_description, chk_result->return_code );
    } else {
        logger( GM_DEBUG, "host job completed: %s: %d\n", chk_result->host_name, chk_result->return_code );
    }

    // nagios internal function
    add_check_result_to_list( chk_result );

    // reset pointer
    chk_result = NULL;

    // give gearman something to free
    uint8_t *buffer;
    buffer = malloc( 1 );
    return buffer;
}
