/******************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/


/* include header */
#include "common.h"
#include "worker.h"
#include "worker_client.h"
#include "utils.h"
#include "worker_logger.h"
#include "gearman.h"

char temp_buffer1[GM_BUFFERSIZE];
char temp_buffer2[GM_BUFFERSIZE];
char hostname[GM_BUFFERSIZE];

gearman_worker_st worker;
gearman_client_st client;

int number_jobs_done = 0;

gm_job_t * current_job;
pid_t current_child_pid;
gm_job_t * exec_job;

int sleep_time_after_error = 1;
int worker_run_mode;


/* callback for task completed */
void worker_client(int worker_mode) {

    logger( GM_LOG_TRACE, "worker client started\n" );

    /* set signal handlers for a clean exit */
    signal(SIGINT, clean_worker_exit);
    signal(SIGTERM,clean_worker_exit);

    worker_run_mode = worker_mode;
    exec_job    = ( gm_job_t * )malloc( sizeof *exec_job );

    // create worker
    if(set_worker(&worker) != GM_OK) {
        logger( GM_LOG_ERROR, "cannot start worker\n" );
        exit( EXIT_FAILURE );
    }

    // create client
    if ( create_client( mod_gm_opt_server, &client ) != GM_OK ) {
        logger( GM_LOG_ERROR, "cannot start client\n" );
        exit( EXIT_FAILURE );
    }

    gethostname(hostname, GM_BUFFERSIZE-1);

    worker_loop();

    return;
}

/* main loop of jobs */
void worker_loop() {

    while ( 1 ) {
        gearman_return_t ret;

        // wait three minutes for a job, otherwise exit
        if(worker_run_mode != GM_WORKER_STANDALONE)
            alarm(180);

        ret = gearman_worker_work( &worker );
        if ( ret != GEARMAN_SUCCESS ) {
            logger( GM_LOG_ERROR, "worker error: %s\n", gearman_worker_error( &worker ) );
            gearman_job_free_all( &worker );
            gearman_worker_free( &worker );
            gearman_client_free( &client );

            // sleep on error to avoid cpu intensive infinite loops
            sleep(sleep_time_after_error);
            sleep_time_after_error += 3;
            if(sleep_time_after_error > 60)
                sleep_time_after_error = 60;

            // create new connections
            set_worker( &worker );
            create_client( mod_gm_opt_server, &client );
        }
    }

    return;
}


/* get a job */
void *get_job( gearman_job_st *job, void *context, size_t *result_size, gearman_return_t *ret_ptr ) {

    // reset timeout for now, will be set befor execution again
    alarm(0);

    logger( GM_LOG_TRACE, "get_job()\n" );

    // contect is unused
    context = context;

    // set size of result
    *result_size = 0;

    // reset sleep time
    sleep_time_after_error = 1;

    // ignore sigterms while running job
    sigset_t block_mask;
    sigset_t old_mask;
    sigaddset(&block_mask, SIGTERM);
    // TODO: Syscall param rt_sigprocmask(set) points to uninitialised byte(s)
    sigprocmask(SIG_BLOCK, &block_mask, &old_mask);

    // send start signal to parent
    send_state_to_parent(GM_JOB_START);

    // get the data
    char * workload = strdup((char*)gearman_job_workload(job));
    logger( GM_LOG_DEBUG, "got new job %s\n", gearman_job_handle( job ) );
    logger( GM_LOG_TRACE, "%d +++>\n%s\n<+++\n", strlen(workload), workload );

    // decrypt data
    char * decrypted_data = malloc(GM_BUFFERSIZE);
    char * decrypted_data_c = decrypted_data;
    mod_gm_decrypt(&decrypted_data, workload, mod_gm_transportmode);
    free(workload);

    if(decrypted_data == NULL) {
        *ret_ptr = GEARMAN_WORK_FAIL;
        return NULL;
    }
    logger( GM_LOG_TRACE, "%d --->\n%s\n<---\n", strlen(decrypted_data), decrypted_data );

    // set result pointer to success
    *ret_ptr= GEARMAN_SUCCESS;

    exec_job->type                = NULL;
    exec_job->host_name           = NULL;
    exec_job->service_description = NULL;
    exec_job->result_queue        = NULL;
    exec_job->command_line        = NULL;
    exec_job->output              = NULL;
    exec_job->exited_ok           = TRUE;
    exec_job->scheduled_check     = TRUE;
    exec_job->reschedule_check    = TRUE;
    exec_job->return_code         = STATE_OK;
    exec_job->latency             = 0.0;
    exec_job->timeout             = mod_gm_opt_timeout;
    exec_job->start_time.tv_sec   = 0L;
    exec_job->start_time.tv_usec  = 0L;

    char *ptr;
    char command[GM_BUFFERSIZE];
    while ( (ptr = strsep(&decrypted_data, "\n" )) != NULL ) {
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
            exec_job->latency = atof(value);
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
        } else if ( !strcmp( key, "command_line" ) ) {
            snprintf(command, sizeof(command), "%s 2>&1", value);
            exec_job->command_line = command;
        }
    }

    do_exec_job();

    // send finish signal to parent
    send_state_to_parent(GM_JOB_END);

    // start listening to SIGTERMs
    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    free(decrypted_data_c);

    return NULL;
}


/* do some job */
void do_exec_job( ) {
    logger( GM_LOG_TRACE, "do_exec_job()\n" );

    struct timeval start_time,end_time;
    double latency = 0.0;

    if(exec_job->type == NULL) {
        logger( GM_LOG_ERROR, "discarded invalid job\n" );
        return;
    }
    if(exec_job->command_line == NULL) {
        logger( GM_LOG_ERROR, "discarded invalid job\n" );
        return;
    }

    logger( GM_LOG_TRACE, "timeout %i\n", exec_job->timeout);

    // calculate real latency
    // get the check start time
    gettimeofday(&start_time,NULL);
    if(exec_job->start_time.tv_sec == 0) {
        exec_job->start_time = start_time;
    }

    // our gm start time
    double start1_f = start_time.tv_sec + start_time.tv_usec/1000000;

    // start time from core
    double start2_f = exec_job->start_time.tv_sec + exec_job->start_time.tv_usec / 1000000;

    latency = start1_f - start2_f;
    logger( GM_LOG_TRACE, "latency: %0.4f\n", latency);
    exec_job->latency += latency;
    if(latency < 0) {
        logger( GM_LOG_ERROR, "latency: %0.4f\n", latency);
        time_t real_start = (time_t) exec_job->start_time.tv_sec;
        logger( GM_LOG_ERROR, "real start_time: %i.%i\n", exec_job->start_time.tv_sec, exec_job->start_time.tv_usec);
        logger( GM_LOG_ERROR, "real start_time: %s\n", asctime(localtime(&real_start)));
        time_t start = (time_t) start_time.tv_sec;
        logger( GM_LOG_ERROR, "job start_time: %i.%i\n", start_time.tv_sec, start_time.tv_usec);
        logger( GM_LOG_ERROR, "job start_time: %s\n", asctime(localtime(&start)));
    }

    // job is too old
    if((int)exec_job->latency > mod_gm_opt_max_age) {
        exec_job->return_code   = 3;

        logger( GM_LOG_INFO, "discarded too old %s job: %i > %i\n", exec_job->type, (int)latency, mod_gm_opt_max_age);

        gettimeofday(&end_time, NULL);
        exec_job->finish_time = end_time;

        if ( !strcmp( exec_job->type, "service" ) || !strcmp( exec_job->type, "host" ) ) {
            exec_job->output = "(Could Not Start Check In Time)";
            send_result_back();
        }

        return;
    }

    exec_job->early_timeout = 0;

    // run the command
    logger( GM_LOG_TRACE, "command: %s\n", exec_job->command_line);
    execute_safe_command();

    // record check result info
    gettimeofday(&end_time, NULL);
    exec_job->finish_time = end_time;

    time_t end = time(&end_time.tv_sec);
    logger( GM_LOG_TRACE, "finish_time: %i.%i\n", end_time.tv_sec, end_time.tv_usec);
    logger( GM_LOG_TRACE, "finish_time: %s\n", asctime(localtime(&end)));

    // did we have a timeout?
    if(exec_job->timeout < ((int)end_time.tv_sec - (int)start_time.tv_sec)) {
        exec_job->return_code   = 2;
        exec_job->early_timeout = 1;
        if ( !strcmp( exec_job->type, "service" ) )
            exec_job->output = "(Service Check Timed Out)";
        if ( !strcmp( exec_job->type, "host" ) )
            exec_job->output = "(Host Check Timed Out)";
    }

    if ( !strcmp( exec_job->type, "service" ) || !strcmp( exec_job->type, "host" ) ) {
        send_result_back();
    }

    return;
}


/* execute this command with given timeout */
void execute_safe_command() {
    logger( GM_LOG_TRACE, "execute_safe_command()\n" );

    int pdes[2];
    pipe(pdes);

    // fork a child process
    current_child_pid=fork();

    //fork error
    if( current_child_pid == -1 ) {
        exec_job->output      = "(Error On Fork)";
        exec_job->return_code = 3;
        return;
    }

    // we are in the child process
    else if( current_child_pid == 0 ){

        // become the process group leader
        setpgid(0,0);
        current_child_pid = getpid();

        // remove all customn signal handler
        sigset_t mask;
        sigfillset(&mask);
        sigprocmask(SIG_UNBLOCK, &mask, NULL);

        close(pdes[0]);
        signal(SIGALRM, alarm_sighandler);
        alarm(exec_job->timeout);

        // run the plugin check command
        FILE *fp = NULL;
        fp = popen(exec_job->command_line, "r");
        if( fp == NULL ) {
            exit(3);
        }

        // get all lines of plugin output - escape newlines
        char buffer[GM_BUFFERSIZE] = "";
        strcpy(buffer,"");
        char temp_buffer[GM_BUFFERSIZE];
        strcpy(temp_buffer,"");
        while(fgets(buffer,sizeof(buffer)-1,fp)){
            char * buf;
            buf = escape_newlines(buffer);
            strncat(temp_buffer, buf, sizeof( temp_buffer ));
            free(buf);
        }
        write(pdes[1], temp_buffer, strlen(temp_buffer)+1);

        // close the process
        int pclose_result;
        pclose_result = pclose(fp);

        if(pclose_result == -1) {
            char error[GM_BUFFERSIZE];
            snprintf(error, sizeof(error), "error: %s", strerror(errno));
            write(pdes[1], error, strlen(error)+1);
        }

        int return_code = real_exit_code(pclose_result);
        exit(return_code);
    }

    // we are the parent
    else {
        close(pdes[1]);

        logger( GM_LOG_DEBUG, "started check with pid: %d\n", current_child_pid);

        int status;
        waitpid(current_child_pid, &status, 0);
        status = real_exit_code(status);
        logger( GM_LOG_DEBUG, "finished check from pid: %d with status: %d\n", current_child_pid, status);

        // get all lines of plugin output
        char buffer[GM_BUFFERSIZE];
        read(pdes[0], buffer, sizeof(buffer)-1);

        // file not found errors?
        if(status == 127) {
            status = STATE_CRITICAL;
            strncat( buffer, "check was running on node ", sizeof( buffer ));
            strncat( buffer, hostname, sizeof( buffer ));
        }
        exec_job->output = buffer;
        exec_job->return_code = status;
        close(pdes[0]);
    }
    alarm(0);
    current_child_pid = 0;

    return;
}


/* send results back */
void send_result_back() {
    logger( GM_LOG_TRACE, "send_result_back()\n" );

    if(exec_job->result_queue == NULL) {
        return;
    }
    if(exec_job->output == NULL) {
        return;
    }

    logger( GM_LOG_TRACE, "queue: %s\n", exec_job->result_queue );
    temp_buffer1[0]='\x0';
    snprintf( temp_buffer1, sizeof( temp_buffer1 )-1, "host_name=%s\nstart_time=%i.%i\nfinish_time=%i.%i\nlatency=%f\nreturn_code=%i\nexited_ok=%i\n",
              exec_job->host_name,
              ( int )exec_job->start_time.tv_sec,
              ( int )exec_job->start_time.tv_usec,
              ( int )exec_job->finish_time.tv_sec,
              ( int )exec_job->finish_time.tv_usec,
              exec_job->latency,
              exec_job->return_code,
              exec_job->exited_ok
            );

    if(exec_job->service_description != NULL) {
        temp_buffer2[0]='\x0';
        strncat(temp_buffer2, "service_description=", (sizeof(temp_buffer2)-1));
        strncat(temp_buffer2, exec_job->service_description, (sizeof(temp_buffer2)-1));
        strncat(temp_buffer2, "\n", (sizeof(temp_buffer2)-1));

        strncat(temp_buffer1, temp_buffer2, (sizeof(temp_buffer1)-1));
    }

    if(exec_job->output != NULL) {
        temp_buffer2[0]='\x0';
        strncat(temp_buffer2, "output=", (sizeof(temp_buffer2)-1));
        if(mod_gm_opt_debug_result) {
            strncat(temp_buffer2, "(", (sizeof(temp_buffer2)-1));
            strncat(temp_buffer2, hostname, (sizeof(temp_buffer2)-1));
            strncat(temp_buffer2, ") - ", (sizeof(temp_buffer2)-1));
        }
        strncat(temp_buffer2, exec_job->output, (sizeof(temp_buffer2)-1));
        strncat(temp_buffer2, "\n", (sizeof(temp_buffer2)-1));
        strncat(temp_buffer1, temp_buffer2, (sizeof(temp_buffer1)-1));
    }
    strncat(temp_buffer1, "\n", (sizeof(temp_buffer1)-1));

    logger( GM_LOG_DEBUG, "data:\n%s\n", temp_buffer1);

    if(add_job_to_queue( &client,
                         exec_job->result_queue,
                         NULL,
                         temp_buffer1,
                         GM_JOB_PRIO_NORMAL,
                         GM_DEFAULT_JOB_RETRIES,
                         mod_gm_transportmode
                        ) == GM_OK) {
        logger( GM_LOG_TRACE, "send_result_back() finished successfully\n" );
    }

    return;
}


/* create the worker */
int set_worker( gearman_worker_st *worker ) {
    logger( GM_LOG_TRACE, "set_worker()\n" );

    create_worker( mod_gm_opt_server, worker );

    if(mod_gm_opt_hosts == GM_ENABLED)
        worker_add_function( worker, "host", get_job );

    if(mod_gm_opt_services == GM_ENABLED)
        worker_add_function( worker, "service", get_job );

    if(mod_gm_opt_events == GM_ENABLED)
        worker_add_function( worker, "eventhandler", get_job );

    int x = 0;
    while ( mod_gm_hostgroups_list[x] != NULL ) {
        char buffer[GM_BUFFERSIZE];
        snprintf( buffer, (sizeof(buffer)-1), "hostgroup_%s", mod_gm_hostgroups_list[x] );
        worker_add_function( worker, buffer, get_job );
        x++;
    }

    x = 0;
    while ( mod_gm_servicegroups_list[x] != NULL ) {
        char buffer[GM_BUFFERSIZE];
        snprintf( buffer, (sizeof(buffer)-1), "servicegroup_%s", mod_gm_servicegroups_list[x] );
        worker_add_function( worker, buffer, get_job );
        x++;
    }

    // add our dummy queue, gearman sometimes forgets the last added queue
    worker_add_function( worker, "dummy", dummy);

    return GM_OK;
}


/* called when check runs into timeout */
void alarm_sighandler(int sig) {
    logger( GM_LOG_TRACE, "alarm_sighandler(%i)\n", sig );

    pid_t pid = getpid();
    signal(SIGINT, SIG_IGN);
    logger( GM_LOG_TRACE, "send SIGINT to %d\n", pid);
    kill(-pid, SIGINT);
    signal(SIGINT, SIG_DFL);
    sleep(1);
    logger( GM_LOG_TRACE, "send SIGKILL to %d\n", pid);
    kill(-pid, SIGKILL);

    if(worker_run_mode != GM_WORKER_STANDALONE)
        _exit(EXIT_SUCCESS);

    return;
}

/* tell parent our state */
void send_state_to_parent(int status) {
    logger( GM_LOG_TRACE, "send_state_to_parent(%d)\n", status );

    if(worker_run_mode == GM_WORKER_STANDALONE)
        return;

    int shmid;
    int *shm;

    // Locate the segment.
    if ((shmid = shmget(gm_shm_key, GM_SHM_SIZE, 0666)) < 0) {
        perror("shmget");
        logger( GM_LOG_DEBUG, "worker finished: %d\n", getpid() );
        exit( EXIT_FAILURE );
    }

    // Now we attach the segment to our data space.
    if ((shm = shmat(shmid, NULL, 0)) == (int *) -1) {
        perror("shmat");
        logger( GM_LOG_DEBUG, "worker finished: %d\n", getpid() );
        exit( EXIT_FAILURE );
    }

    // set our counter
    if(status == GM_JOB_START)
        shm[0]++;
    if(status == GM_JOB_END)
        shm[0]--;

    // detach from shared memory
    if(shmdt(shm) < 0)
        perror("shmdt");

    // wake up parent
    kill(getppid(), SIGUSR1);

    if(number_jobs_done >= GM_MAX_JOBS_PER_CLIENT) {
        logger( GM_LOG_DEBUG, "worker finished: %d\n", getpid() );
        exit(EXIT_SUCCESS);
    }

    return;
}


/* do a clean exit */
void clean_worker_exit(int sig) {
    logger( GM_LOG_TRACE, "clean_worker_exit(%d)\n", sig);

    exit( EXIT_SUCCESS );
}
