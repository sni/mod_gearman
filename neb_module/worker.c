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
void *result_worker(void * args) {

    gearman_return_t ret;
    gearman_worker_st worker;

    result_worker_arg opt = *(result_worker_arg *) args;


    if (gearman_worker_create(&worker) == NULL) {
        logger(GM_ERROR, "Memory allocation failure on worker creation\n");
        return;
    }

    int x = 0;
    while(opt.server[x] != NULL) {
        char * server  = strdup(opt.server[x]);
        x++;
        if(strchr(server, ':') == NULL) {
            break;
        };
        char * host    = str_token(&server, ':');
        in_port_t port = (in_port_t) atoi(str_token(&server, 0));
        ret=gearman_worker_add_server(&worker, host, port);
        if (ret != GEARMAN_SUCCESS) {
            logger(GM_ERROR, "%s\n", gearman_worker_error(&worker));
            return;
        }
        logger(GM_DEBUG, "added gearman server %s:%i\n", host, port);
    }

    logger(GM_DEBUG, "started result_worker thread for queue: %s\n", gearman_opt_result_queue);

    if(gearman_opt_result_queue == NULL) {
        logger(GM_ERROR, "got no result queue!\n");
        exit(1);
    }

    ret = gearman_worker_add_function(&worker, gearman_opt_result_queue, 0, get_results, NULL);
          gearman_worker_add_function(&worker, "blah", 0, get_results, NULL); // somehow the last function is ignored, so in order to set the first one active. Add a useless one
    if (ret != GEARMAN_SUCCESS) {
        logger(GM_ERROR, "worker error: %s\n", gearman_worker_error(&worker));
        return;
    }

    while (1) {
        ret = gearman_worker_work(&worker);
        if (ret != GEARMAN_SUCCESS) {
            logger(GM_ERROR, "worker error: %s\n", gearman_worker_error(&worker));
            break;
        }
        gearman_job_free_all(&worker);
    }

    gearman_worker_free(&worker);

    return;
}


void *get_results(gearman_job_st *job, void *context, size_t *result_size, gearman_return_t *ret_ptr) {

    // always set result pointer to success
    *ret_ptr= GEARMAN_SUCCESS;

    char *workload;
    workload= strdup((char *)gearman_job_workload(job));
    *result_size= gearman_job_workload_size(job);

    logger(GM_TRACE, "got result\n--->\n%s\n<---\n", workload);

    // nagios will free it after processing
    check_result * result;
    if ((result = (check_result *)malloc(sizeof *result)) == 0)
        return GEARMAN_SUCCESS;

    result->service_description = NULL;

    char *ptr;
    while(ptr = strsep(&workload, "\n")) {
        char *key   = str_token(&ptr, '=');
        char *value = str_token(&ptr, 0);

        if(key == NULL)
            continue;

        if(value == NULL) {
            break;
        }

        if(value == "") {
            logger(GM_ERROR, "got empty value for key %s\n", key);
            continue;
        }

        if(!strcmp(key, "host_name")) {
            result->host_name = strdup(value);
        }
        else if(!strcmp(key, "service_description")) {
            result->service_description = strdup(value);
        }
        else if(!strcmp(key, "check_options")) {
            result->check_options = atoi(value);
        }
        else if(!strcmp(key, "scheduled_check")) {
            result->scheduled_check = atoi(value);
        }
        else if(!strcmp(key, "reschedule_check")) {
            result->reschedule_check = atoi(value);
        }
        else if(!strcmp(key, "exited_ok")) {
            result->exited_ok = atoi(value);
        }
        else if(!strcmp(key, "early_timeout")) {
            result->early_timeout = atoi(value);
        }
        else if(!strcmp(key, "return_code")) {
            result->return_code = atoi(value);
        }
        else if(!strcmp(key, "output")) {
            result->output = strdup(value);
        }
        else if(!strcmp(key, "start_time")) {
            int sec   = atoi(str_token(&value, '.'));
            int usec  = atoi(str_token(&value, 0));
            result->start_time.tv_sec    = sec;
            result->start_time.tv_usec   = usec;
        }
        else if(!strcmp(key, "finish_time")) {
            int sec   = atoi(str_token(&value, '.'));
            int usec  = atoi(str_token(&value, 0));
            result->finish_time.tv_sec    = sec;
            result->finish_time.tv_usec   = usec;
        }
        else if(!strcmp(key, "latency")) {
            result->latency = atof(value);
        }
    }

    if(result == NULL)
        return GEARMAN_SUCCESS;

    if(result->host_name == NULL)
        return GEARMAN_SUCCESS;

    if(result->service_description != NULL) {
        result->object_check_type    = SERVICE_CHECK;
        result->check_type           = SERVICE_CHECK_ACTIVE;
    } else {
        result->object_check_type    = HOST_CHECK;
        result->check_type           = HOST_CHECK_ACTIVE;
    }

    // initialize and fill with result info
    result->output_file    = 0;
    result->output_file_fd = -1;

    if(result->service_description != NULL) {
        logger(GM_DEBUG, "service job completed: %s %s: %d\n", result->host_name, result->service_description, result->return_code);
    } else {
        logger(GM_DEBUG, "host job completed: %s: %d\n", result->host_name, result->return_code);
    }

    // nagios internal function
    add_check_result_to_list(result);

    /* reset pointer */
    result = NULL;

    return GEARMAN_SUCCESS;
}
