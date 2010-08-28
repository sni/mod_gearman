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

char temp_buffer1[GM_BUFFERSIZE];
char temp_buffer2[GM_BUFFERSIZE];
char output_buffer[GM_BUFFERSIZE];
char hostname[GM_BUFFERSIZE];
gearman_client_st client;
gearman_worker_st worker;

int number_jobs_done = 0;

gm_job_t * current_job;

/* callback for task completed */
void *worker_client( ) {

    logger( GM_LOG_TRACE, "worker client started\n" );

    // create gearman worker
    if(create_gearman_worker(&worker) != GM_OK) {
        logger( GM_LOG_ERROR, "cannot start worker\n" );
        exit( EXIT_FAILURE );
    }

    // create gearman client
    if ( create_gearman_client(&client) != GM_OK ) {
        logger( GM_LOG_ERROR, "cannot start client\n" );
        exit( EXIT_FAILURE );
    }

    gethostname(hostname, GM_BUFFERSIZE-1);

    worker_loop();

    return NULL;
}

/* main loop of jobs */
void *worker_loop() {

    while ( 1 ) {
        gearman_return_t ret;
        ret = gearman_worker_work( &worker );
        if ( ret != GEARMAN_SUCCESS ) {
            logger( GM_LOG_ERROR, "worker error: %s\n", gearman_worker_error( &worker ) );
            gearman_job_free_all( &worker );
            gearman_worker_free( &worker );
            create_gearman_worker( &worker );

            gearman_client_free( &client );
            create_gearman_client( &client );
        }
    }

    return NULL;
}


/* get a job */
void *get_job( gearman_job_st *job, void *context, size_t *result_size, gearman_return_t *ret_ptr ) {
    logger( GM_LOG_TRACE, "get_job()\n" );

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
        exit(EXIT_FAILURE);
     }

    // send start signal to parent
    send_state_to_parent(GM_JOB_START);

    snprintf( result, ( int )*result_size, "%.*s", ( int )*result_size, workload );
    logger( GM_LOG_DEBUG, "got new job %s%s%s\n", gearman_job_handle( job ),
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
    //    logger( GM_LOG_TRACE, "discarding non data request\n" );
    //    *result_size= 0;
    //    return NULL;
    //}

    gm_job_t * exec_job;
    if ( ( exec_job = ( gm_job_t * )malloc( sizeof *exec_job ) ) == 0 ) {
        // send finish signal to parent
        send_state_to_parent(GM_JOB_END);

        *ret_ptr= GEARMAN_WORK_FAIL;
        exit(EXIT_FAILURE);
    }
    current_job = exec_job;
    exec_job->type                = NULL;
    exec_job->host_name           = NULL;
    exec_job->service_description = NULL;
    exec_job->result_queue        = NULL;
    exec_job->exited_ok           = 1;
    exec_job->scheduled_check     = 1;
    exec_job->reschedule_check    = 1;
    exec_job->return_code         = 0;
    exec_job->latency             = 0;
    exec_job->timeout             = gearman_opt_timeout;
    exec_job->start_time.tv_sec   = 0L;
    exec_job->start_time.tv_usec  = 0L;

    char *ptr;
    while ( (ptr = strsep(&result, "\n" )) != NULL ) {
        char *key   = str_token( &ptr, '=' );
        char *value = str_token( &ptr, 0 );

        if ( key == NULL )
            continue;

        if ( value == NULL || !strcmp( value, "") )
            continue;

        if ( !strcmp( key, "host_name" ) ) {
            exec_job->host_name = value;
        } else if ( !strcmp( key, "service_description" ) ) {
            exec_job->service_description = value;
        } else if ( !strcmp( key, "type" ) ) {
            exec_job->type = value;
        } else if ( !strcmp( key, "result_queue" ) ) {
            exec_job->result_queue = value;
        } else if ( !strcmp( key, "check_options" ) ) {
            exec_job->check_options = atoi(value);
        } else if ( !strcmp( key, "scheduled_check" ) ) {
            exec_job->scheduled_check = atoi(value);
        } else if ( !strcmp( key, "reschedule_check" ) ) {
            exec_job->reschedule_check = atoi(value);
        } else if ( !strcmp( key, "latency" ) ) {
            exec_job->latency = atoi(value);
        } else if ( !strcmp( key, "start_time" ) ) {
            char *ptr   = strdup(value);
            char *ptr_c = ptr;
            char *sec   = str_token( &ptr, '.' );
            char *usec  = str_token( &ptr, 0 );
            if(usec == NULL) { usec = 0; }
            exec_job->start_time.tv_sec  = atoi(sec);
            exec_job->start_time.tv_usec = atoi(usec);
            free(ptr_c);
        } else if ( !strcmp( key, "timeout" ) ) {
            exec_job->timeout = atoi(value);
        } else if ( !strcmp( key, "latency" ) ) {
            exec_job->latency = atof(value);
        } else if ( !strcmp( key, "command_line" ) ) {
            exec_job->command_line = value;
        }
    }

    do_exec_job(exec_job);

    free(result_c);

    // send finish signal to parent
    send_state_to_parent(GM_JOB_END);

    // give gearman something to free
    uint8_t *buffer;
    buffer = malloc( 1 );
    return buffer;
}


/* do some job */
void *do_exec_job( gm_job_t *job ) {
    logger( GM_LOG_TRACE, "do_exec_job()\n" );

    pid_t pid;
    FILE *fp = NULL;
    int pclose_result;
    struct timeval start_time,end_time;
    double latency = 0.0;

    if(job->type == NULL) {
        return;
    }
    if(job->command_line == NULL) {
        return;
    }

    logger( GM_LOG_TRACE, "timeout %i\n", job->timeout);

    // calculate real latency
    // get the check start time
    gettimeofday(&start_time,NULL);
    if(job->start_time.tv_sec == 0) {
        job->start_time = start_time;
    }
    time_t real_start = time(&job->start_time.tv_sec);
    logger( GM_LOG_TRACE, "real start_time: %i.%i\n", job->start_time.tv_sec, job->start_time.tv_usec);
    logger( GM_LOG_TRACE, "real start_time: %s\n", asctime(localtime(&real_start)));

    time_t start = time(&start_time.tv_sec);
    logger( GM_LOG_TRACE, "job start_time: %i.%i\n", start_time.tv_sec, start_time.tv_usec);
    logger( GM_LOG_TRACE, "job start_time: %s\n", asctime(localtime(&start)));

    temp_buffer1[0]='\x0';
    snprintf(temp_buffer1, (sizeof(temp_buffer1)), "%i.%i", start_time.tv_sec, start_time.tv_usec);
    double start1_f = atof(temp_buffer1);
    temp_buffer1[0]='\x0';
    snprintf(temp_buffer1, (sizeof(temp_buffer1)), "%i.%i", job->start_time.tv_sec, job->start_time.tv_usec);
    double start2_f = atof(temp_buffer1);
    latency = start1_f - start2_f;
    logger( GM_LOG_TRACE, "latency: %0.4f\n", latency);
    job->latency = latency;
    if(job->latency < 0) { job->latency = 0; }
    // job is too old
    if((int)job->latency > gearman_opt_max_age) {
        current_job->return_code   = 3;

        logger( GM_LOG_INFO, "discarded too old %s job: %i > %i\n", job->type, (int)latency, gearman_opt_max_age);

        gettimeofday(&end_time, NULL);
        current_job->finish_time   = end_time;

        if ( !strcmp( current_job->type, "service" ) || !strcmp( current_job->type, "host" ) ) {
            current_job->output = "(Could Not Start Check In Time)";
            send_result_back(current_job);
        }

        return;
    }


/*
    // fork a child process
    pid=fork();

    //fork error
    if( pid == -1 ) {
        job->output      = "(Error On Fork)";
        job->return_code = 3;
        job->exited_ok   = 0;
        if ( !strcmp( job->type, "service" ) || !strcmp( job->type, "host" ) ) {
            send_result_back(job);
        }
        exit(EXIT_FAILURE);
    }

    // we are in the child process
    else if(pid==0){
*/
        signal(SIGALRM, alarm_sighandler);
        alarm(job->timeout);

        // run the plugin check command
        logger( GM_LOG_TRACE, "running cmd: %s\n", job->command_line);
        fp = popen(job->command_line, "r");
        if( fp == NULL ) {
            job->output      = "(Error On Exec)";
            job->return_code = 3;
            job->exited_ok   = 0;
            if ( !strcmp( job->type, "service" ) || !strcmp( job->type, "host" ) ) {
                send_result_back(job);
            }
            return;
        }

        // initialize buffer
        strcpy(output_buffer,"");

        // get all lines of plugin output - escape newlines
        fgets(output_buffer,sizeof(output_buffer)-1,fp);
        job->output = escape_newlines(output_buffer);

        // close the process
        pclose_result = pclose(fp);

        // reset the alarm
        alarm(0);

        // record check result info
        gettimeofday(&end_time, NULL);
        job->finish_time   = end_time;
        job->early_timeout = 0;

        time_t end = time(&end_time.tv_sec);
        logger( GM_LOG_TRACE, "finish_time: %i.%i\n", end_time.tv_sec, end_time.tv_usec);
        logger( GM_LOG_TRACE, "finish_time: %s\n", asctime(localtime(&end)));

        // test for execution error
        if(pclose_result == -1){
            pclose_result    = 3;
            job->return_code = 2;
            job->exited_ok   = 0;
        }
        else {
            if(WEXITSTATUS(pclose_result)==0 && WIFSIGNALED(pclose_result))
                job->return_code=128+WTERMSIG(pclose_result);
            else
                job->return_code=WEXITSTATUS(pclose_result);
            }

        if ( !strcmp( job->type, "service" ) || !strcmp( job->type, "host" ) ) {
            send_result_back(job);
        }

        signal(SIGALRM, SIG_DFL);

        //exit(EXIT_SUCCESS);
/*
    }
    // we are the parent
    else {
        logger( GM_LOG_DEBUG, "started check with pid: %d\n", pid);
        int status;
        waitpid(pid, &status, 0);
        logger( GM_LOG_DEBUG, "finished check from pid: %d with status: %d\n", pid, status);
    }
*/

    return;
}


/* send results back */
void *send_result_back( gm_job_t *job ) {
    logger( GM_LOG_TRACE, "send_result_back()\n" );

    if(job->result_queue == NULL) {
        return;
    }
    if(job->output == NULL) {
        return;
    }

    logger( GM_LOG_TRACE, "queue: %s\n", job->result_queue );
    temp_buffer1[0]='\x0';
    snprintf( temp_buffer1, sizeof( temp_buffer1 )-1, "host_name=%s\nstart_time=%i.%i\nfinish_time=%i.%i\nlatency=%f\nreturn_code=%i\nexited_ok=%i\n",
              job->host_name,
              ( int )job->start_time.tv_sec,
              ( int )job->start_time.tv_usec,
              ( int )job->finish_time.tv_sec,
              ( int )job->finish_time.tv_usec,
              job->latency,
              job->return_code,
              job->exited_ok
            );

    if(job->service_description != NULL) {
        temp_buffer2[0]='\x0';
        strncat(temp_buffer2, "service_description=", (sizeof(temp_buffer2)-1));
        strncat(temp_buffer2, job->service_description, (sizeof(temp_buffer2)-1));
        strncat(temp_buffer2, "\n", (sizeof(temp_buffer2)-1));

        strncat(temp_buffer1, temp_buffer2, (sizeof(temp_buffer1)-1));
    }

    if(job->output != NULL) {
        temp_buffer2[0]='\x0';
        strncat(temp_buffer2, "output=", (sizeof(temp_buffer2)-1));
        if(gearman_opt_debug_result) {
            strncat(temp_buffer2, "(", (sizeof(temp_buffer2)-1));
            strncat(temp_buffer2, hostname, (sizeof(temp_buffer2)-1));
            strncat(temp_buffer2, ") - ", (sizeof(temp_buffer2)-1));
        }
        strncat(temp_buffer2, job->output, (sizeof(temp_buffer2)-1));
        strncat(temp_buffer2, "\n", (sizeof(temp_buffer2)-1));

        strncat(temp_buffer1, temp_buffer2, (sizeof(temp_buffer1)-1));
    }

    gearman_task_st *task = NULL;
    gearman_return_t ret;
    gearman_client_add_task_background( &client, task, NULL, job->result_queue, NULL, ( void * )temp_buffer1, ( size_t )strlen( temp_buffer1 ), &ret );
    gearman_client_run_tasks( &client );
    if(ret != GEARMAN_SUCCESS || (gearman_client_error(&client) != NULL && strcmp(gearman_client_error(&client), "") != 0)) { // errno is somehow empty, use error instead
        logger( GM_LOG_ERROR, "send_result_back() finished with errors: %s\n", gearman_client_error(&client) );
        gearman_client_free(&client);
        sleep(5);
        create_gearman_client( &client );

        // try to resubmit once
        gearman_client_add_task_background( &client, task, NULL, job->result_queue, NULL, ( void * )temp_buffer1, ( size_t )strlen( temp_buffer1 ), &ret );
        gearman_client_run_tasks( &client );
        if(ret != GEARMAN_SUCCESS || (gearman_client_error(&client) != NULL && strcmp(gearman_client_error(&client), "") != 0)) { // errno is somehow empty, use error instead
            logger( GM_LOG_DEBUG, "client error permanent: %s\n", gearman_client_error(&client));
            gearman_client_free(&client);
            create_gearman_client(&client);
        }
        else {
            logger( GM_LOG_DEBUG, "retransmission successful\n");
        }

        return;
    }

    logger( GM_LOG_TRACE, "send_result_back() finished successfully\n" );

    return;
}


/* create the gearman worker */
int create_gearman_worker( gearman_worker_st *worker ) {
    logger( GM_LOG_TRACE, "create_gearman_worker()\n" );

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


/* create the gearman worker */
int create_gearman_client( gearman_client_st *client ) {
    logger( GM_LOG_TRACE, "create_gearman_client()\n" );

    gearman_return_t ret;

    if ( gearman_client_create( client ) == NULL ) {
        logger( GM_LOG_ERROR, "Memory allocation failure on client creation\n" );
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
        ret = gearman_client_add_server( client, host, port );
        if ( ret != GEARMAN_SUCCESS ) {
            logger( GM_LOG_ERROR, "client error: %s\n", gearman_client_error( client ) );
            free(server_c);
            return GM_ERROR;
        }
        free(server_c);
        x++;
    }

    return GM_OK;
}

/* called when check runs into timeout */
void alarm_sighandler() {
    logger( GM_LOG_TRACE, "alarm_sighandler()\n" );

    struct timeval end_time;

    if ( !strcmp( current_job->type, "service" ) || !strcmp( current_job->type, "host" ) ) {
        current_job->early_timeout = 1;
        current_job->return_code   = 2;

        gettimeofday(&end_time, NULL);
        current_job->finish_time   = end_time;

        if ( !strcmp( current_job->type, "service" ) )
            current_job->output = "(Service Check Timed Out)";

        if ( !strcmp( current_job->type, "host" ) )
            current_job->output = "(Host Check Timed Out)";

        send_result_back(current_job);
    }

    // send finish signal to parent
    send_state_to_parent(GM_JOB_END);

    // become the process group leader and kill all childs
    setpgid(0,0);
    //signal(SIGINT, SIG_IGN);
    //signal(SIGTERM, SIG_IGN);
    kill((pid_t)0, SIGTERM);
    logger( GM_LOG_TRACE, "send SIGTERM\n");
    sleep(1);
    kill((pid_t)0, SIGINT);
    logger( GM_LOG_TRACE, "send SIGINT\n");

    //signal(SIGINT, SIG_DFL);
    //signal(SIGTERM, SIG_DFL);

    _exit(EXIT_SUCCESS);
}

/* tell parent our state */
void send_state_to_parent(int status) {
    logger( GM_LOG_TRACE, "send_state_to_parent(%d)\n", status );

    if(status == GM_JOB_START) {
        number_jobs_done++;
        kill(getppid(), SIGUSR1);
    } else {
        kill(getppid(), SIGUSR2);
        if(number_jobs_done >= GM_MAX_JOBS_PER_CLIENT) {
            exit(EXIT_SUCCESS);
        }
    }
}
