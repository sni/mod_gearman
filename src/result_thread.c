/******************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/


/* include header */
#include "result_thread.h"
#include "utils.h"
#include "mod_gearman.h"
#include "logger.h"

static int create_gearman_worker( gearman_worker_st *);

/* cleanup and exit this thread */
static void cancel_worker_thread (void * data) {

    gearman_worker_st *worker = (gearman_worker_st*) data;

    gearman_worker_unregister_all(worker);
    gearman_worker_remove_servers(worker);
    gearman_worker_free(worker);

    logger( GM_LOG_DEBUG, "worker thread finished\n" );

    return;
}

/* callback for task completed */
void *result_worker( void * data ) {

    worker_parm *p=(worker_parm *)data;
    logger( GM_LOG_TRACE, "worker %i started\n", p->id );

    pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    gearman_worker_st worker;
    create_gearman_worker(&worker);

    pthread_cleanup_push ( cancel_worker_thread, (void*) &worker);

    while ( 1 ) {
        gearman_return_t ret;
        ret = gearman_worker_work( &worker );
        if ( ret != GEARMAN_SUCCESS ) {
            logger( GM_LOG_ERROR, "worker error: %s\n", gearman_worker_error( &worker ) );
            gearman_job_free_all( &worker );
            gearman_worker_free( &worker );

            sleep(1);
            create_gearman_worker( &worker );
        }
    }

    pthread_cleanup_pop(0);
    return NULL;
}

/* put back the result into the core */
void *get_results( gearman_job_st *job, void *context, size_t *result_size, gearman_return_t *ret_ptr ) {

    gm_worker_options_t options= *( ( gm_worker_options_t * )context );

    // get the data
    const uint8_t *workload;
    workload= gearman_job_workload( job );
    *result_size= gearman_job_workload_size( job );

    char *result;
    result = malloc( *result_size );
    char *result_c = result;
    if ( result == NULL ) {
        logger( GM_LOG_ERROR, "malloc:%d\n", errno );
        *ret_ptr= GEARMAN_WORK_FAIL;
        return NULL;
    }

    snprintf( result, ( int )*result_size, "%.*s", ( int )*result_size, workload );
    logger( GM_LOG_DEBUG, "got result %s%s%s\n", gearman_job_handle( job ),
            options & GM_WORKER_OPTIONS_UNIQUE ? " Unique=" : "",
            options & GM_WORKER_OPTIONS_UNIQUE ? gearman_job_unique( job ) : ""
          );
    logger( GM_LOG_TRACE, "--->\n%.*s\n<---\n", ( int )*result_size, result );

    logger( GM_LOG_TRACE, "options none  : %s\n", options & GM_WORKER_OPTIONS_NONE   ? "yes" : "no"),
    logger( GM_LOG_TRACE, "options data  : %s\n", options & GM_WORKER_OPTIONS_DATA   ? "yes" : "no"),
    logger( GM_LOG_TRACE, "options status: %s\n", options & GM_WORKER_OPTIONS_STATUS ? "yes" : "no"),

    // set result pointer to success
    *ret_ptr= GEARMAN_SUCCESS;
    // TODO: verify this
    //if ( ! options & GM_WORKER_OPTIONS_DATA ) {
    //    logger( GM_LOG_TRACE, "discarding non data event\n" );
    //    *result_size= 0;
    //    return NULL;
    //}

    // nagios will free it after processing
    check_result * chk_result;
    if ( ( chk_result = ( check_result * )malloc( sizeof *chk_result ) ) == 0 ) {
        *ret_ptr= GEARMAN_WORK_FAIL;
        return NULL;
    }

    chk_result->service_description = NULL;
    chk_result->host_name           = NULL;
    chk_result->output              = NULL;
    chk_result->return_code         = 0;
    chk_result->exited_ok           = 1;
    chk_result->early_timeout       = 0;
    chk_result->latency             = 0;
    chk_result->start_time.tv_sec   = 0;
    chk_result->start_time.tv_usec  = 0;
    chk_result->finish_time.tv_sec  = 0;
    chk_result->finish_time.tv_usec = 0;
    chk_result->scheduled_check     = TRUE;
    chk_result->reschedule_check    = TRUE;

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
    free(result_c);

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

    // this check is not a freshnes check
    chk_result->check_options    = chk_result->check_options & ! CHECK_OPTION_FRESHNESS_CHECK;

    // initialize and fill with result info
    chk_result->output_file    = 0;
    chk_result->output_file_fd = -1;

    // fill some maybe missing options
    if(chk_result->start_time.tv_sec  == 0) {
        chk_result->start_time.tv_sec = (unsigned long)time(NULL);
    }
    if(chk_result->finish_time.tv_sec  == 0) {
        chk_result->finish_time.tv_sec = (unsigned long)time(NULL);
    }

    if ( chk_result->service_description != NULL ) {
        logger( GM_LOG_DEBUG, "service job completed: %s %s: %d\n", chk_result->host_name, chk_result->service_description, chk_result->return_code );
    } else {
        logger( GM_LOG_DEBUG, "host job completed: %s: %d\n", chk_result->host_name, chk_result->return_code );
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


/* create the gearman worker */
static int create_gearman_worker( gearman_worker_st *worker ) {

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
        logger( GM_LOG_DEBUG, "worker added gearman server %s:%i\n", host, port );
        free(server_c);
        x++;
    }

    if ( gearman_opt_result_queue == NULL ) {
        logger( GM_LOG_ERROR, "got no result queue!\n" );
        return GM_ERROR;
    }
    logger( GM_LOG_DEBUG, "started result_worker thread for queue: %s\n", gearman_opt_result_queue );

    ret = gearman_worker_add_function( worker, gearman_opt_result_queue, 0, get_results, &options );
    // add it once again, sometime the first one cannot register
    ret = gearman_worker_add_function( worker, gearman_opt_result_queue, 0, get_results, &options );
    if ( ret != GEARMAN_SUCCESS ) {
        logger( GM_LOG_ERROR, "worker error: %s\n", gearman_worker_error( worker ) );
        return GM_ERROR;
    }
    return OK;
}
