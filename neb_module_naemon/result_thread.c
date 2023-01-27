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


/* include header */
#include "result_thread.h"
#include "utils.h"
#include "mod_gearman.h"
#include "gearman_utils.h"

extern mod_gm_opt_t *mod_gm_opt;
extern char hostname[GM_SMALLBUFSIZE];
extern unsigned long total_submit_jobs;
extern unsigned long total_submit_errors;
extern float current_submit_rate;
extern float current_avg_submit_duration;
extern double current_submit_max;
extern int result_threads_running;

EVP_CIPHER_CTX * result_ctx = NULL;

static const char *gearman_worker_source_name(void *source) {
    if(!source)
        return "unknown internal source (voodoo, perhaps?)";

    // we cannot return the source here as it would be never freed
    //return (char*) source;
    return "Mod-Gearman Worker";
}

static struct check_engine mod_gearman_check_engine = {
    "Mod-Gearman",
    gearman_worker_source_name,
    NULL
};

/* cleanup and exit this thread */
static void cancel_worker_thread (void * data) {

    if(data != NULL) {
        gearman_worker_st **worker = (gearman_worker_st**) data;
        gm_free_worker(worker);
    }

    mod_gm_crypt_deinit(result_ctx);
    gm_log( GM_LOG_DEBUG, "worker thread finished\n" );

    return;
}

/* callback for task completed */
void *result_worker( void * data ) {
    gearman_worker_st *worker = NULL;
    int *worker_num = (int*)data;
    gearman_return_t ret;

    gm_log( GM_LOG_TRACE, "worker %d started\n", *worker_num );
    gethostname(hostname, GM_SMALLBUFSIZE-1);

    pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    set_worker(&worker);
    pthread_cleanup_push(cancel_worker_thread, (void*) &worker);

    result_ctx = mod_gm_crypt_init(mod_gm_opt->crypt_key);

    while ( 1 ) {
        ret = gearman_worker_work(worker);
        if ( ret != GEARMAN_SUCCESS && ret != GEARMAN_WORK_FAIL ) {
            if ( ret != GEARMAN_TIMEOUT)
                gm_log( GM_LOG_ERROR, "worker error: %s\n", gearman_worker_error(worker));

            gm_free_worker(&worker);
            sleep(1);

            set_worker(&worker);
        }
    }

    pthread_cleanup_pop(0);
    return NULL;
}

/* put back the result into the core */
void *get_results( gearman_job_st *job, __attribute__((__unused__)) void *context, size_t *result_size, gearman_return_t *ret_ptr ) {
    int transportmode;
    const char *workload;
    char *decrypted_data = NULL;
    char *decrypted_data_c;
    struct timeval now, core_start_time;
    check_result * chk_result;
    int active_check = TRUE;
    char *ptr;
    double now_f, core_starttime_f, starttime_f, finishtime_f, exec_time, latency;
    size_t wsize = 0;

    /* for calculating real latency */
    gettimeofday(&now,NULL);

    /* set size of result */
    *result_size = 0;

    /* set result pointer to success */
    *ret_ptr = GEARMAN_SUCCESS;

    /* get the data */
    wsize = gearman_job_workload_size(job);
    workload = (const char *)gearman_job_workload(job);
    if(workload == NULL) {
        *ret_ptr = GEARMAN_WORK_FAIL;
        return NULL;
    }
    gm_log( GM_LOG_TRACE, "got result %s\n", gearman_job_handle(job));
    gm_log( GM_LOG_TRACE, "%zu +++>\n%.*s\n<+++\n", wsize, wsize, workload );

    /* decrypt data */
    if(mod_gm_opt->transportmode == GM_ENCODE_AND_ENCRYPT && mod_gm_opt->accept_clear_results == GM_ENABLED) {
        transportmode = GM_ENCODE_ACCEPT_ALL;
    } else {
        transportmode = mod_gm_opt->transportmode;
    }
    mod_gm_decrypt(result_ctx, &decrypted_data, workload, wsize, transportmode);
    decrypted_data_c = decrypted_data;

    if(!strcmp(workload, "check")) {
        char * result = gm_malloc(GM_BUFFERSIZE);
        *result_size = GM_BUFFERSIZE;
        snprintf(result, GM_BUFFERSIZE, "0:OK - result worker running on %s. Sending %.1f jobs/s (avg duration:%.3fms). Version: %s|worker=%i;;;0;%i avg_submit_duration=%.6fs;;;0;%.6f jobs=%luc errors=%luc",
                                            hostname,
                                            current_submit_rate,
                                            (current_avg_submit_duration*1000),
                                            GM_VERSION,
                                            result_threads_running,
                                            mod_gm_opt->result_workers,
                                            current_avg_submit_duration,
                                            current_submit_max,
                                            total_submit_jobs,
                                            total_submit_errors
        );
        return((void*)result);
    }

    if(decrypted_data == NULL) {
        *ret_ptr = GEARMAN_WORK_FAIL;
        return NULL;
    }
    gm_log( GM_LOG_TRACE, "%zu --->\n%s\n<---\n", strlen(decrypted_data), decrypted_data );

    /*
     * save this result to a file, so when nagios crashes,
     * we have at least the crashed package
     */
#ifdef GM_DEBUG
    if(mod_gm_opt->debug_result == GM_ENABLED) {
        FILE * fd;
        fd = fopen( "/tmp/last_result_received.txt", "w+" );
        if(fd == NULL) {
            perror("fopen");
        } else {
            fputs( decrypted_data, fd );
            fclose( fd );
        }
    }
#endif

    /* naemon will free it after processing */
    if ( ( chk_result = ( check_result * )gm_malloc( sizeof *chk_result ) ) == 0 ) {
        *ret_ptr = GEARMAN_WORK_FAIL;
        gm_free(decrypted_data_c);
        return NULL;
    }
    init_check_result(chk_result);
    chk_result->scheduled_check     = TRUE;
    chk_result->output_file         = 0;
    chk_result->output_file_fp      = NULL;
    chk_result->engine              = &mod_gearman_check_engine;
    core_start_time.tv_sec          = 0;
    core_start_time.tv_usec         = 0;
    chk_result->latency             = 0;

    while ( (ptr = strsep(&decrypted_data, "\n" )) != NULL ) {
        char *key   = strsep( &ptr, "=" );
        char *value = strsep( &ptr, "\x0" );

        if ( key == NULL )
            continue;

        if ( !strcmp( key, "output" ) ) {
            if ( value == NULL ) {
                chk_result->output = gm_strdup("(null)");
            }
            else {

                char *tmp_newline = replace_str(value, "\\n", "\n");
                if (tmp_newline == NULL)
                    chk_result->output = gm_strdup("(null)");

                char *tmp_backslash = replace_str(tmp_newline, "\\\\", "\\");
                if (tmp_backslash == NULL)
                    chk_result->output = gm_strdup("(null)");
                else
                    chk_result->output = gm_strdup( tmp_backslash );

                gm_free(tmp_newline);
                gm_free(tmp_backslash);
            }
        }

        if ( value == NULL || !strcmp( value, "") )
            break;

        if ( !strcmp( key, "host_name" ) ) {
            chk_result->host_name = gm_strdup( value );
        } else if ( !strcmp( key, "service_description" ) ) {
            chk_result->service_description = gm_strdup( value );
        } else if ( !strcmp( key, "source" ) ) {
            chk_result->source = value;
        } else if ( !strcmp( key, "check_options" ) ) {
            chk_result->check_options = atoi( value );
        } else if ( !strcmp( key, "scheduled_check" ) ) {
            chk_result->scheduled_check = atoi( value );
        } else if ( !strcmp( key, "type" ) && !strcmp( value, "passive" ) ) {
            active_check=FALSE;
        } else if ( !strcmp( key, "exited_ok" ) ) {
            chk_result->exited_ok = atoi( value );
        } else if ( !strcmp( key, "early_timeout" ) ) {
            chk_result->early_timeout = atoi( value );
        } else if ( !strcmp( key, "return_code" ) ) {
            chk_result->return_code = atoi( value );
        } else if ( !strcmp( key, "core_start_time" ) ) {
            string2timeval(value, &core_start_time);
        } else if ( !strcmp( key, "start_time" ) ) {
            string2timeval(value, &chk_result->start_time);
        } else if ( !strcmp( key, "finish_time" ) ) {
            string2timeval(value, &chk_result->finish_time);
        } else if ( !strcmp( key, "latency" ) ) { // used by send_gearman
            chk_result->latency = atof( value );
        }
    }

    if ( chk_result->host_name == NULL || chk_result->output == NULL ) {
        *ret_ptr= GEARMAN_WORK_FAIL;
        gm_log( GM_LOG_ERROR, "discarded invalid job (%s), check your encryption settings\n", gearman_job_handle( job ) );
        return NULL;
    }

    if ( chk_result->service_description != NULL ) {
        chk_result->object_check_type    = SERVICE_CHECK;
        chk_result->check_type           = SERVICE_CHECK_ACTIVE;
        if(active_check == FALSE )
            chk_result->check_type       = SERVICE_CHECK_PASSIVE;
    } else {
        chk_result->object_check_type    = HOST_CHECK;
        chk_result->check_type           = HOST_CHECK_ACTIVE;
        if(active_check == FALSE )
            chk_result->check_type       = HOST_CHECK_PASSIVE;
    }

    /* fill some maybe missing options */
    if(chk_result->start_time.tv_sec  == 0) {
        chk_result->start_time.tv_sec = (unsigned long)time(NULL);
    }
    if(chk_result->finish_time.tv_sec  == 0) {
        chk_result->finish_time.tv_sec = (unsigned long)time(NULL);
    }
    if(core_start_time.tv_sec  == 0) {
        core_start_time.tv_sec = (unsigned long)time(NULL);
    }

    /* calculate real latency */
    now_f            = timeval2double(&now);
    core_starttime_f = timeval2double(&core_start_time);            // time before job was sent to gearmand
    starttime_f      = timeval2double(&chk_result->start_time);     // ts when check started on worker
    finishtime_f     = timeval2double(&chk_result->finish_time);    // ts when check finished on worker
    exec_time        = finishtime_f - starttime_f;
    latency          = now_f - exec_time - core_starttime_f;

    if(latency < 0)
        latency = 0;
    if(chk_result->latency < 0)
        chk_result->latency = 0;

    chk_result->latency += latency;

    /* this check is not a freshnes check */
    chk_result->check_options    = chk_result->check_options & ! CHECK_OPTION_FRESHNESS_CHECK;

    if ( chk_result->service_description != NULL ) {
        gm_log( GM_LOG_DEBUG, "service job completed: %s %s: exit %d, latency: %0.3f, exec_time: %0.3f\n", chk_result->host_name, chk_result->service_description, chk_result->return_code, chk_result->latency, exec_time );
    } else {
        if(active_check) {
            host * hst = find_host( chk_result->host_name );
            if(hst != NULL) {
                hst->is_executing = FALSE;
            }
        }
        gm_log( GM_LOG_DEBUG, "host job completed: %s: exit %d, latency: %0.3f, exec_time: %0.3f\n", chk_result->host_name, chk_result->return_code, chk_result->latency, exec_time );
    }

    /* add result to result list */
    mod_gm_add_result_to_list( chk_result );

    /* reset pointer */
    chk_result = NULL;

    gm_free(decrypted_data_c);

    return NULL;
}


/* get the worker */
int set_worker( gearman_worker_st **worker ) {
    gm_free_worker(worker);

    gearman_worker_st *w = create_worker(mod_gm_opt->server_list);
    if(w == NULL) {
        return GM_ERROR;
    }
    *worker = w;

    if ( mod_gm_opt->result_queue == NULL ) {
        gm_log( GM_LOG_ERROR, "got no result queue!\n" );
        return GM_ERROR;
    }
    gm_log( GM_LOG_DEBUG, "started result_worker thread for queue: %s\n", mod_gm_opt->result_queue );

    if(worker_add_function( w, mod_gm_opt->result_queue, get_results ) != GM_OK) {
        return GM_ERROR;
    }

    /* add our dummy queue, gearman sometimes forgets the last added queue */
    worker_add_function( w, "dummy", dummy);

    /* let our worker renew itself every 30 seconds */
    if(mod_gm_opt->server_num > 1)
        gearman_worker_set_timeout(w, 30000);

    return GM_OK;
}
