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
#include "worker.h"
#include "common.h"
#include "worker_client.h"
#include "utils.h"
#include "worker_logger.h"
#include "gearman.h"

char temp_buffer1[GM_BUFFERSIZE];
char temp_buffer2[GM_BUFFERSIZE];
char hostname[GM_BUFFERSIZE];

gearman_worker_st worker;
gearman_client_st client;

gm_job_t * current_job;
pid_t current_child_pid;
gm_job_t * exec_job;

int jobs_done = 0;
int sleep_time_after_error = 1;
int worker_run_mode;


/* callback for task completed */
void worker_client(int worker_mode) {

    logger( GM_LOG_TRACE, "worker client started\n" );

    /* set signal handlers for a clean exit */
    signal(SIGINT, clean_worker_exit);
    signal(SIGTERM,clean_worker_exit);

    worker_run_mode = worker_mode;

    gethostname(hostname, GM_BUFFERSIZE-1);

    /* create worker */
    if(set_worker(&worker) != GM_OK) {
        logger( GM_LOG_ERROR, "cannot start worker\n" );
        exit( EXIT_FAILURE );
    }

    /* create client */
    if ( create_client( mod_gm_opt->server_list, &client ) != GM_OK ) {
        logger( GM_LOG_ERROR, "cannot start client\n" );
        exit( EXIT_FAILURE );
    }

    worker_loop();

    return;
}


/* main loop of jobs */
void worker_loop() {

    while ( 1 ) {
        gearman_return_t ret;

        /* wait three minutes for a job, otherwise exit */
        if(worker_run_mode == GM_WORKER_MULTI)
            alarm(mod_gm_opt->idle_timeout);

        signal(SIGPIPE, SIG_IGN);
        ret = gearman_worker_work( &worker );
        if ( ret != GEARMAN_SUCCESS ) {
            logger( GM_LOG_ERROR, "worker error: %s\n", gearman_worker_error( &worker ) );
            gearman_job_free_all( &worker );
            gearman_worker_free( &worker );
            gearman_client_free( &client );

            /* sleep on error to avoid cpu intensive infinite loops */
            sleep(sleep_time_after_error);
            sleep_time_after_error += 3;
            if(sleep_time_after_error > 60)
                sleep_time_after_error = 60;

            /* create new connections */
            set_worker( &worker );
            create_client( mod_gm_opt->server_list, &client );
        }
    }

    return;
}


/* get a job */
void *get_job( gearman_job_st *job, void *context, size_t *result_size, gearman_return_t *ret_ptr ) {
    sigset_t block_mask;
    sigset_t old_mask;
    int wsize;
    char workload[GM_BUFFERSIZE];
    char * decrypted_data;
    char * decrypted_data_c;
    char * decrypted_orig;
    char *ptr;
    char command[GM_BUFFERSIZE];

    jobs_done++;

    /* send start signal to parent */
    send_state_to_parent(GM_JOB_START);

    /* reset timeout for now, will be set befor execution again */
    alarm(0);

    logger( GM_LOG_TRACE, "get_job()\n" );

    /* contect is unused */
    context = context;

    /* set size of result */
    *result_size = 0;

    /* reset sleep time */
    sleep_time_after_error = 1;

    /* ignore sigterms while running job */
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &block_mask, &old_mask);

    /* get the data */
    wsize = gearman_job_workload_size(job);
    strncpy(workload, (const char*)gearman_job_workload(job), wsize);
    workload[wsize] = '\0';
    logger( GM_LOG_TRACE, "got new job %s\n", gearman_job_handle( job ) );
    logger( GM_LOG_TRACE, "%d +++>\n%s\n<+++\n", strlen(workload), workload );

    /* decrypt data */
    decrypted_data = malloc(GM_BUFFERSIZE);
    decrypted_data_c = decrypted_data;
    mod_gm_decrypt(&decrypted_data, workload, mod_gm_opt->transportmode);
    decrypted_orig = strdup(decrypted_data);

    if(decrypted_data == NULL) {
        *ret_ptr = GEARMAN_WORK_FAIL;
        return NULL;
    }
    logger( GM_LOG_TRACE, "%d --->\n%s\n<---\n", strlen(decrypted_data), decrypted_data );

    /* set result pointer to success */
    *ret_ptr= GEARMAN_SUCCESS;

    exec_job = ( gm_job_t * )malloc( sizeof *exec_job );
    set_default_job(exec_job);

    while ( (ptr = strsep(&decrypted_data, "\n" )) != NULL ) {
        char *key   = strsep( &ptr, "=" );
        char *value = strsep( &ptr, "\x0" );

        if ( key == NULL )
            continue;

        if ( value == NULL || !strcmp( value, "") )
            break;

        if ( !strcmp( key, "host_name" ) ) {
            exec_job->host_name = strdup(value);
        } else if ( !strcmp( key, "service_description" ) ) {
            exec_job->service_description = strdup(value);
        } else if ( !strcmp( key, "type" ) ) {
            exec_job->type = strdup(value);
        } else if ( !strcmp( key, "result_queue" ) ) {
            exec_job->result_queue = strdup(value);
        } else if ( !strcmp( key, "check_options" ) ) {
            exec_job->check_options = atoi(value);
        } else if ( !strcmp( key, "scheduled_check" ) ) {
            exec_job->scheduled_check = atoi(value);
        } else if ( !strcmp( key, "reschedule_check" ) ) {
            exec_job->reschedule_check = atoi(value);
        } else if ( !strcmp( key, "latency" ) ) {
            exec_job->latency = atof(value);
        } else if ( !strcmp( key, "start_time" ) ) {
            string2timeval(value, &exec_job->core_start_time);
        } else if ( !strcmp( key, "timeout" ) ) {
            exec_job->timeout = atoi(value);
        } else if ( !strcmp( key, "command_line" ) ) {
            snprintf(command, sizeof(command), "%s 2>&1", value);
            exec_job->command_line = strdup(command);
        }
    }

#ifdef GM_DEBUG
    if(exec_job->core_start_time.tv_sec < 10000)
        write_debug_file(&decrypted_orig);
#endif

    do_exec_job();

    /* start listening to SIGTERMs */
    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    free(decrypted_orig);
    free(decrypted_data_c);
    free_job(exec_job);

    /* send finish signal to parent */
    send_state_to_parent(GM_JOB_END);

    if(jobs_done >= mod_gm_opt->max_jobs) {
        logger( GM_LOG_TRACE, "jobs done: %i -> exiting...\n", jobs_done );
        gearman_worker_unregister_all(&worker);
        gearman_job_free_all( &worker );
        gearman_client_free( &client );
        mod_gm_free_opt(mod_gm_opt);
        exit( EXIT_SUCCESS );
    }

    return NULL;
}


/* do some job */
void do_exec_job( ) {
    struct timeval start_time,end_time;
    int latency;
    char plugin_output[GM_BUFFERSIZE];
    strcpy(plugin_output,"");

    logger( GM_LOG_TRACE, "do_exec_job()\n" );

    if(exec_job->type == NULL) {
        logger( GM_LOG_ERROR, "discarded invalid job\n" );
        return;
    }
    if(exec_job->command_line == NULL) {
        logger( GM_LOG_ERROR, "discarded invalid job\n" );
        return;
    }

    if ( !strcmp( exec_job->type, "service" ) ) {
        logger( GM_LOG_DEBUG, "got service job: %s - %s\n", exec_job->host_name, exec_job->service_description);
    }
    else if ( !strcmp( exec_job->type, "host" ) ) {
        logger( GM_LOG_DEBUG, "got host job: %s\n", exec_job->host_name);
    }
    else if ( !strcmp( exec_job->type, "event" ) ) {
        logger( GM_LOG_DEBUG, "got eventhandler job\n");
    }

    /* check proper timeout value */
    if( exec_job->timeout <= 0 ) {
        exec_job->timeout = mod_gm_opt->job_timeout;
    }

    logger( GM_LOG_TRACE, "timeout %i\n", exec_job->timeout);

    /* get the check start time */
    gettimeofday(&start_time,NULL);
    exec_job->start_time = start_time;
    latency = exec_job->core_start_time.tv_sec - start_time.tv_sec;

    /* job is too old */
    if(latency > mod_gm_opt->max_age) {
        exec_job->return_code   = 3;

        logger( GM_LOG_INFO, "discarded too old %s job: %i > %i\n", exec_job->type, (int)latency, mod_gm_opt->max_age);

        gettimeofday(&end_time, NULL);
        exec_job->finish_time = end_time;

        if ( !strcmp( exec_job->type, "service" ) || !strcmp( exec_job->type, "host" ) ) {
            exec_job->output = strdup("(Could Not Start Check In Time)");
            send_result_back();
        }

        return;
    }

    exec_job->early_timeout = 0;

    /* run the command */
    logger( GM_LOG_TRACE, "command: %s\n", exec_job->command_line);
    execute_safe_command();

    /* record check result info */
    gettimeofday(&end_time, NULL);
    exec_job->finish_time = end_time;

    /* did we have a timeout? */
    if(exec_job->timeout < ((int)end_time.tv_sec - (int)start_time.tv_sec)) {
        exec_job->return_code   = 2;
        exec_job->early_timeout = 1;
        if ( !strcmp( exec_job->type, "service" ) )
            snprintf( plugin_output, sizeof( plugin_output ), "(Service Check Timed Out On Worker: %s)", hostname);
        if ( !strcmp( exec_job->type, "host" ) )
            snprintf( plugin_output, sizeof( plugin_output ), "(Host Check Timed Out On Worker: %s)", hostname);
        exec_job->output = strdup( plugin_output );
    }

    if ( !strcmp( exec_job->type, "service" ) || !strcmp( exec_job->type, "host" ) ) {
        send_result_back();
    }

    return;
}


/* execute this command with given timeout */
void execute_safe_command() {
    int pdes[2];
    int return_code;
    int pclose_result;
    int fork_exec = mod_gm_opt->fork_on_exec;
    char *plugin_output;
    char buffer[GM_BUFFERSIZE];
    sigset_t mask;

    logger( GM_LOG_TRACE, "execute_safe_command()\n" );

    /* fork a child process */
    if(fork_exec == GM_ENABLED) {
        if(pipe(pdes) != 0)
            perror("pipe");

        current_child_pid=fork();

        /*fork error */
        if( current_child_pid == -1 ) {
            exec_job->output      = strdup("(Error On Fork)");
            exec_job->return_code = 3;
            return;
        }
    }

    /* we are in the child process */
    if( fork_exec == GM_DISABLED || current_child_pid == 0 ){

        /* become the process group leader */
        setpgid(0,0);
        current_child_pid = getpid();

        /* remove all customn signal handler */
        sigfillset(&mask);
        sigprocmask(SIG_UNBLOCK, &mask, NULL);

        if( fork_exec == GM_ENABLED )
            close(pdes[0]);
        signal(SIGALRM, alarm_sighandler);
        alarm(exec_job->timeout);

        /* run the plugin check command */
        pclose_result = run_check(exec_job->command_line, &plugin_output);
        return_code   = real_exit_code(pclose_result);

        if(fork_exec == GM_ENABLED) {
            if(write(pdes[1], plugin_output, strlen(plugin_output)+1) <= 0)
                perror("write");

            if(pclose_result == -1) {
                char error[GM_BUFFERSIZE];
                snprintf(error, sizeof(error), "error on %s: %s", hostname, strerror(errno));
                if(write(pdes[1], error, strlen(error)+1) <= 0)
                    perror("write");
            }

            exit(return_code);
        }
        else {
            snprintf( buffer, sizeof( buffer )-1, "%s", plugin_output );
        }
    }

    /* we are the parent */
    if( fork_exec == GM_DISABLED || current_child_pid > 0 ){

        logger( GM_LOG_TRACE, "started check with pid: %d\n", current_child_pid);

        if( fork_exec == GM_ENABLED) {
            close(pdes[1]);

            waitpid(current_child_pid, &return_code, 0);
            return_code = real_exit_code(return_code);
            logger( GM_LOG_TRACE, "finished check from pid: %d with status: %d\n", current_child_pid, return_code);
            /* get all lines of plugin output */
            if(read(pdes[0], buffer, sizeof(buffer)-1) < 0)
                perror("read");
        }

        /* file not executable? */
        if(return_code == 126) {
            return_code = STATE_CRITICAL;
            snprintf( buffer, sizeof( buffer )-1, "CRITICAL: Return code of 126 is out of bounds. Make sure the plugin you're trying to run is executable. (worker: %s)", hostname);
        }
        /* file not found errors? */
        else if(return_code == 127) {
            return_code = STATE_CRITICAL;
            snprintf( buffer, sizeof( buffer )-1, "CRITICAL: Return code of 127 is out of bounds. Make sure the plugin you're trying to run actually exists. (worker: %s)", hostname);
        }
        /* signaled */
        else if(return_code >= 128 && return_code < 256) {
            char * signame = nr2signal((int)(return_code-128));
            snprintf( buffer, sizeof( buffer )-1, "CRITICAL: Return code of %d is out of bounds. Plugin exited by signal %s. (worker: %s)", (int)(return_code), signame, hostname);
            return_code = STATE_CRITICAL;
            free(signame);
        }
        exec_job->output      = strdup(buffer);
        exec_job->return_code = return_code;
        if( fork_exec == GM_ENABLED) {
            close(pdes[0]);
        }
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
    snprintf( temp_buffer1, sizeof( temp_buffer1 )-1, "host_name=%s\ncore_start_time=%i.%i\nstart_time=%i.%i\nfinish_time=%i.%i\nlatency=%f\nreturn_code=%i\nexited_ok=%i\n",
              exec_job->host_name,
              ( int )exec_job->core_start_time.tv_sec,
              ( int )exec_job->core_start_time.tv_usec,
              ( int )exec_job->start_time.tv_sec,
              ( int )exec_job->start_time.tv_usec,
              ( int )exec_job->finish_time.tv_sec,
              ( int )exec_job->finish_time.tv_usec,
              exec_job->latency,
              exec_job->return_code,
              exec_job->exited_ok
            );
    temp_buffer1[sizeof( temp_buffer1 )-1]='\x0';

    if(exec_job->service_description != NULL) {
        temp_buffer2[0]='\x0';
        strncat(temp_buffer2, "service_description=", (sizeof(temp_buffer2)-1));
        strncat(temp_buffer2, exec_job->service_description, (sizeof(temp_buffer2)-1));
        strncat(temp_buffer2, "\n", (sizeof(temp_buffer2)-1));

        strncat(temp_buffer1, temp_buffer2, (sizeof(temp_buffer1)-1));
    }
    temp_buffer1[sizeof( temp_buffer1 )-1]='\x0';

    if(exec_job->output != NULL) {
        temp_buffer2[0]='\x0';
        strncat(temp_buffer2, "output=", (sizeof(temp_buffer2)-1));
        if(mod_gm_opt->debug_result) {
            strncat(temp_buffer2, "(", (sizeof(temp_buffer2)-1));
            strncat(temp_buffer2, hostname, (sizeof(temp_buffer2)-1));
            strncat(temp_buffer2, ") - ", (sizeof(temp_buffer2)-1));
        }
        strncat(temp_buffer2, exec_job->output, (sizeof(temp_buffer2)-1));
        strncat(temp_buffer2, "\n\n\n", (sizeof(temp_buffer2)-1));
        strncat(temp_buffer1, temp_buffer2, (sizeof(temp_buffer1)-1));
    }
    strncat(temp_buffer1, "\n", (sizeof(temp_buffer1)-2));
    temp_buffer1[sizeof( temp_buffer1 )-1]='\x0';

    logger( GM_LOG_TRACE, "data:\n%s\n", temp_buffer1);

    if(add_job_to_queue( &client,
                         mod_gm_opt->server_list,
                         exec_job->result_queue,
                         NULL,
                         temp_buffer1,
                         GM_JOB_PRIO_NORMAL,
                         GM_DEFAULT_JOB_RETRIES,
                         mod_gm_opt->transportmode
                        ) == GM_OK) {
        logger( GM_LOG_TRACE, "send_result_back() finished successfully\n" );
    }
    else {
        logger( GM_LOG_TRACE, "send_result_back() finished unsuccessfully\n" );
    }

    return;
}


/* create the worker */
int set_worker( gearman_worker_st *w ) {
    int x = 0;

    logger( GM_LOG_TRACE, "set_worker()\n" );

    create_worker( mod_gm_opt->server_list, w );

    if(worker_run_mode == GM_WORKER_STATUS) {
        /* register status function */
        char status_queue[GM_BUFFERSIZE];
        snprintf(status_queue, GM_BUFFERSIZE, "worker_%s", hostname);
        worker_add_function( w, status_queue, return_status );
    }
    else {
        /* normal worker */
        if(mod_gm_opt->hosts == GM_ENABLED)
            worker_add_function( w, "host", get_job );

        if(mod_gm_opt->services == GM_ENABLED)
            worker_add_function( w, "service", get_job );

        if(mod_gm_opt->events == GM_ENABLED)
            worker_add_function( w, "eventhandler", get_job );

        while ( mod_gm_opt->hostgroups_list[x] != NULL ) {
            char buffer[GM_BUFFERSIZE];
            snprintf( buffer, (sizeof(buffer)-1), "hostgroup_%s", mod_gm_opt->hostgroups_list[x] );
            worker_add_function( w, buffer, get_job );
            x++;
        }

        x = 0;
        while ( mod_gm_opt->servicegroups_list[x] != NULL ) {
            char buffer[GM_BUFFERSIZE];
            snprintf( buffer, (sizeof(buffer)-1), "servicegroup_%s", mod_gm_opt->servicegroups_list[x] );
            worker_add_function( w, buffer, get_job );
            x++;
        }
    }

    /* add our dummy queue, gearman sometimes forgets the last added queue */
    worker_add_function( w, "dummy", dummy);

    return GM_OK;
}


/* called when check runs into timeout */
void alarm_sighandler(int sig) {
    pid_t pid = getpid();

    logger( GM_LOG_TRACE, "alarm_sighandler(%i)\n", sig );

    signal(SIGINT, SIG_IGN);
    logger( GM_LOG_TRACE, "send SIGINT to %d\n", pid);
    kill(-pid, SIGINT);
    signal(SIGINT, SIG_DFL);
    sleep(1);
    logger( GM_LOG_TRACE, "send SIGKILL to %d\n", pid);
    kill(-pid, SIGKILL);

    if(worker_run_mode != GM_WORKER_STANDALONE)
        exit(EXIT_SUCCESS);

    return;
}

/* tell parent our state */
void send_state_to_parent(int status) {
    int shmid;
    int *shm;

    logger( GM_LOG_TRACE, "send_state_to_parent(%d)\n", status );

    /* Locate the segment */
    if ((shmid = shmget(mod_gm_shm_key, GM_SHM_SIZE, 0600)) < 0) {
        perror("shmget");
        logger( GM_LOG_TRACE, "worker finished: %d\n", getpid() );
        exit( EXIT_FAILURE );
    }

    /* Now we attach the segment to our data space. */
    if ((shm = shmat(shmid, NULL, 0)) == (int *) -1) {
        perror("shmat");
        logger( GM_LOG_TRACE, "worker finished: %d\n", getpid() );
        exit( EXIT_FAILURE );
    }

    /* set our counter */
    if(status == GM_JOB_START)
        shm[0]++;
    if(status == GM_JOB_END) {
        shm[0]--;
        shm[2]++; /* increase jobs done */
    }

    /* detach from shared memory */
    if(shmdt(shm) < 0)
        perror("shmdt");

    if(worker_run_mode != GM_WORKER_MULTI)
        return;

    /* wake up parent */
    kill(getppid(), SIGUSR1);

    return;
}


/* do a clean exit */
void clean_worker_exit(int sig) {
    int shmid;

    logger( GM_LOG_TRACE, "clean_worker_exit(%d)\n", sig);

    gearman_worker_unregister_all(&worker);
    gearman_job_free_all( &worker );
    gearman_client_free( &client );

    mod_gm_free_opt(mod_gm_opt);

    if(worker_run_mode != GM_WORKER_STANDALONE)
        exit( EXIT_SUCCESS );

    /*
     * clean up shared memory
     * will be removed when last client detaches
     */
    if ((shmid = shmget(mod_gm_shm_key, GM_SHM_SIZE, 0600)) < 0) {
        perror("shmget");
    }
    if( shmctl( shmid, IPC_RMID, 0 ) == -1 ) {
        perror("shmctl");
    }

    exit( EXIT_SUCCESS );
}


/* answer status querys */
void *return_status( gearman_job_st *job, void *context, size_t *result_size, gearman_return_t *ret_ptr ) {
    int wsize;
    char workload[GM_BUFFERSIZE];
    int shmid;
    int *shm;
    char * result;

    logger( GM_LOG_TRACE, "return_status()\n" );

    /* contect is unused */
    context = context;

    /* get the data */
    wsize = gearman_job_workload_size(job);
    strncpy(workload, (const char*)gearman_job_workload(job), wsize);
    workload[wsize] = '\0';
    logger( GM_LOG_TRACE, "got status job %s\n", gearman_job_handle( job ) );
    logger( GM_LOG_TRACE, "%d +++>\n%s\n<+++\n", strlen(workload), workload );

    /* set result pointer to success */
    *ret_ptr= GEARMAN_SUCCESS;

    /* set size of result */
    result = malloc(GM_BUFFERSIZE);
    *result_size = GM_BUFFERSIZE;

    /* Locate the segment */
    if ((shmid = shmget(mod_gm_shm_key, GM_SHM_SIZE, 0600)) < 0) {
        perror("shmget");
        *result_size = 0;
        return NULL;
    }

    /* Now we attach the segment to our data space. */
    if ((shm = shmat(shmid, NULL, 0)) == (int *) -1) {
        perror("shmat");
        *result_size = 0;
        return NULL;
    }

    snprintf(result, GM_BUFFERSIZE, "%s has %i worker and is working on %i jobs. Version: %s|worker=%i jobs=%ic", hostname, shm[1], shm[0], GM_VERSION, shm[1], shm[2] );

    /* detach from shared memory */
    if(shmdt(shm) < 0)
        perror("shmdt");

    return((void*)result);
}


/* set empty default job */
int set_default_job(gm_job_t *job) {

    job->type                = NULL;
    job->host_name           = NULL;
    job->service_description = NULL;
    job->result_queue        = NULL;
    job->command_line        = NULL;
    job->output              = NULL;
    job->exited_ok           = TRUE;
    job->scheduled_check     = TRUE;
    job->reschedule_check    = TRUE;
    job->return_code         = STATE_OK;
    job->latency             = 0.0;
    job->timeout             = mod_gm_opt->job_timeout;
    job->start_time.tv_sec   = 0L;
    job->start_time.tv_usec  = 0L;

    return(GM_OK);
}


/* free the job structure */
int free_job(gm_job_t *job) {

    free(job->type);
    free(job->host_name);
    free(job->service_description);
    free(job->result_queue);
    free(job->command_line);
    free(job->output);
    free(job);

    return(GM_OK);
}


#ifdef GM_DEBUG
/* write text to a debug file */
void write_debug_file(char ** text) {
    FILE * fd;
    fd = fopen( "/tmp/mod_gearman_worker.txt", "a+" );
    if(fd == NULL)
        perror("fopen");
    fputs( "------------->\n", fd );
    fputs( *text, fd );
    fputs( "\n<-------------\n", fd );
    fclose( fd );
}
#endif

