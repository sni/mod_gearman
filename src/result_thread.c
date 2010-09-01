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
#include "gearman.h"

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
    set_worker(&worker);

    pthread_cleanup_push ( cancel_worker_thread, (void*) &worker);

    while ( 1 ) {
        gearman_return_t ret;
        ret = gearman_worker_work( &worker );
        if ( ret != GEARMAN_SUCCESS ) {
            logger( GM_LOG_ERROR, "worker error: %s\n", gearman_worker_error( &worker ) );
            gearman_job_free_all( &worker );
            gearman_worker_free( &worker );

            sleep(1);
            set_worker(&worker );
        }
    }

    pthread_cleanup_pop(0);
    return NULL;
}

/* put back the result into the core */
void *get_results( gearman_job_st *job, void *context, size_t *result_size, gearman_return_t *ret_ptr ) {

    // contect is unused
    context = context;

    // set size of result
    *result_size = 0;

    // set result pointer to success
    *ret_ptr = GEARMAN_SUCCESS;

    // get the data
    char * workload;
    workload = strdup((char *)gearman_job_workload(job));
    char * workload_c = workload;

    logger( GM_LOG_DEBUG, "got result %s\n", gearman_job_handle( job ));
    logger( GM_LOG_TRACE, "--->\n%s\n<---\n", workload );

    // nagios will free it after processing
    check_result * chk_result;
    if ( ( chk_result = ( check_result * )malloc( sizeof *chk_result ) ) == 0 ) {
        *ret_ptr = GEARMAN_WORK_FAIL;
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
    while ( (ptr = strsep(&workload, "\n" )) != NULL ) {
        char *key   = str_token( &ptr, '=' );
        char *value = str_token( &ptr, 0 );

        if ( key == NULL )
            continue;

        if ( !strcmp( key, "output" ) ) {
            if ( value == NULL ) {
                chk_result->output = strdup("(null)");
            }
            else {
                chk_result->output = strdup( value );
            }
        }

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
    free(workload_c);

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

    return NULL;
}


/* get the worker */
int set_worker( gearman_worker_st *worker ) {

    create_worker( mod_gm_opt_server, worker );

    if ( mod_gm_opt_result_queue == NULL ) {
        logger( GM_LOG_ERROR, "got no result queue!\n" );
        return GM_ERROR;
    }
    logger( GM_LOG_DEBUG, "started result_worker thread for queue: %s\n", mod_gm_opt_result_queue );

    if(worker_add_function( worker, mod_gm_opt_result_queue, get_results ) != GM_OK) {
        return GM_ERROR;
    }

    // add our dummy queue, gearman sometimes forgets the last added queue
    worker_add_function( worker, "dummy", dummy);

    return GM_OK;
}
