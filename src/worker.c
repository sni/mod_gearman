/******************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

/* include header */
#include "worker.h"
#include "utils.h"
#include "worker_logger.h"
#include "worker_client.h"

int gearman_opt_min_worker      = GM_DEFAULT_MIN_WORKER;
int gearman_opt_max_worker      = GM_DEFAULT_MAX_WORKER;
int gearman_opt_max_age         = GM_DEFAULT_JOB_MAX_AGE;
int gearman_opt_timeout         = GM_DEFAULT_TIMEOUT;

int gearman_opt_hosts           = GM_DISABLED;
int gearman_opt_services        = GM_DISABLED;
int gearman_opt_events          = GM_DISABLED;
int gearman_opt_debug_result    = GM_DISABLED;
char *gearman_hostgroups_list[GM_LISTSIZE];
char *gearman_servicegroups_list[GM_LISTSIZE];

int current_number_of_workers                = 0;
volatile sig_atomic_t current_number_of_jobs = 0;  // must be signal safe

/* work starts here */
int main (int argc, char **argv) {

    parse_arguments(argv);
    logger( GM_LOG_DEBUG, "main process started\n");

    if(gearman_opt_max_worker == 1) {
        worker_client(GM_WORKER_STANDALONE);
        exit( EXIT_SUCCESS );
    }

    // Establish the signal handler
    struct sigaction usr_action1;
    sigset_t block_mask;
    sigfillset (&block_mask); // block all signals
    usr_action1.sa_handler = increase_jobs;
    usr_action1.sa_mask    = block_mask;
    usr_action1.sa_flags   = 0;
    sigaction (SIGUSR1, &usr_action1, NULL);

    struct sigaction usr_action2;
    usr_action2.sa_handler = decrease_jobs;
    usr_action2.sa_mask    = block_mask;
    usr_action2.sa_flags   = 0;
    sigaction (SIGUSR2, &usr_action2, NULL);

    // create initial childs
    int x;
    for(x=0; x < gearman_opt_min_worker; x++) {
        make_new_child();
    }

    // maintain the population
    while (1) {
        // check number of workers every 30 seconds
        // sleep gets canceled anyway when receiving signals
        sleep(30);

        // collect finished workers
        int status;
        while(waitpid(-1, &status, WNOHANG) > 0) {
            current_number_of_workers--;
            logger( GM_LOG_TRACE, "waitpid() %d\n", status);
        }

        if(current_number_of_jobs < 0) { current_number_of_jobs = 0; }
        if(current_number_of_jobs > current_number_of_workers) { current_number_of_jobs = current_number_of_workers; }

        // keep up minimum population
        for (x = current_number_of_workers; x < gearman_opt_min_worker; x++) {
            make_new_child();
        }

        int target_number_of_workers = adjust_number_of_worker(gearman_opt_min_worker, gearman_opt_max_worker, current_number_of_workers, current_number_of_jobs);
        for (x = current_number_of_workers; x < target_number_of_workers; x++) {
            // top up the worker pool
            make_new_child();
        }
    }

    exit( EXIT_SUCCESS );
}


/* start up new worker */
int make_new_child() {
    logger( GM_LOG_TRACE, "make_new_child()\n");
    pid_t pid = 0;

    /* fork a child process */
    pid=fork();

    /* an error occurred while trying to fork */
    if(pid==-1){
        logger( GM_LOG_ERROR, "fork error\n" );
        return GM_ERROR;
    }

    /* we are in the child process */
    else if(pid==0){
        logger( GM_LOG_DEBUG, "worker started with pid: %d\n", getpid() );

        signal(SIGUSR1, SIG_IGN);
        signal(SIGUSR2, SIG_IGN);

        // do the real work
        worker_client(GM_WORKER_MULTI);

        logger( GM_LOG_DEBUG, "worker fin: %d\n", getpid() );
        exit(EXIT_SUCCESS);
    }

    /* parent  */
    else if(pid > 0){
        current_number_of_workers++;
    }

    return GM_OK;
}


/* parse command line arguments */
void parse_arguments(char **argv) {
    int x           = 1;
    int srv_ptr     = 0;
    int srvgrp_ptr  = 0;
    int hostgrp_ptr = 0;

    int set_by_hand = 0;

    while(argv[x] != NULL) {
        char *ptr;
        char * args   = strdup( argv[x] );
        while ( (ptr = strsep( &args, " " )) != NULL ) {
            char *key   = str_token( &ptr, '=' );
            char *value = str_token( &ptr, 0 );

            if ( key == NULL )
                continue;

            if ( !strcmp( key, "help" ) || !strcmp( key, "--help" )  || !strcmp( key, "-h" ) ) {
                print_usage();
            }

            if ( !strcmp( key, "hosts" ) || !strcmp( key, "--hosts" ) ) {
                set_by_hand++;
                if( value == NULL || !strcmp( value, "yes" ) ) {
                    gearman_opt_hosts = GM_ENABLED;
                    logger( GM_LOG_DEBUG, "enabling processing of hosts queue\n");
                }
            }
            else if ( !strcmp( key, "services" ) || !strcmp( key, "--services" ) ) {
                set_by_hand++;
                if( value == NULL || !strcmp( value, "yes" ) ) {
                    gearman_opt_services = GM_ENABLED;
                    logger( GM_LOG_DEBUG, "enabling processing of service queue\n");
                }
            }
            else if ( !strcmp( key, "events" ) || !strcmp( key, "--events" ) ) {
                set_by_hand++;
                if( value == NULL || !strcmp( value, "yes" ) ) {
                    gearman_opt_events = GM_ENABLED;
                    logger( GM_LOG_DEBUG, "enabling processing of events queue\n");
                }
            }
            else if ( !strcmp( key, "debug-result" ) || !strcmp( key, "--debug-result" ) ) {
                if( value == NULL || !strcmp( value, "yes" ) ) {
                    gearman_opt_debug_result = GM_ENABLED;
                    logger( GM_LOG_DEBUG, "adding debug output to check output\n");
                }
            }

            if ( value == NULL )
                continue;

            if ( !strcmp( key, "debug" ) || !strcmp( key, "--debug" ) ) {
                gearman_opt_debug_level = atoi( value );
                if(gearman_opt_debug_level < 0) { gearman_opt_debug_level = 0; }
                logger( GM_LOG_DEBUG, "Setting debug level to %d\n", gearman_opt_debug_level );
            }
            else if ( !strcmp( key, "timeout" ) || !strcmp( key, "--timeout" ) ) {
                gearman_opt_timeout = atoi( value );
                if(gearman_opt_timeout < 1) { gearman_opt_timeout = 1; }
                logger( GM_LOG_DEBUG, "Setting default timeout to %d\n", gearman_opt_timeout );
            }
            else if ( !strcmp( key, "min-worker" ) || !strcmp( key, "--min-worker" ) ) {
                gearman_opt_min_worker = atoi( value );
                if(gearman_opt_min_worker <= 0) { gearman_opt_min_worker = 1; }
                logger( GM_LOG_DEBUG, "Setting min worker to %d\n", gearman_opt_min_worker );
            }
            else if ( !strcmp( key, "max-worker" ) || !strcmp( key, "--max-worker" ) ) {
                gearman_opt_max_worker = atoi( value );
                if(gearman_opt_max_worker <= 0) { gearman_opt_max_worker = 1; }
                logger( GM_LOG_DEBUG, "Setting max worker to %d\n", gearman_opt_max_worker );
            }
            else if ( !strcmp( key, "server" ) || !strcmp( key, "--server" ) ) {
                char *servername;
                while ( (servername = strsep( &value, "," )) != NULL ) {
                    if ( strcmp( servername, "" ) ) {
                        logger( GM_LOG_DEBUG, "Adding server %s\n", servername);
                        gearman_opt_server[srv_ptr] = servername;
                        srv_ptr++;
                    }
                }
            }

            else if ( !strcmp( key, "servicegroups" ) || !strcmp( key, "--servicegroups" ) ) {
                char *groupname;
                while ( (groupname = strsep( &value, "," )) != NULL ) {
                    if ( strcmp( groupname, "" ) ) {
                        gearman_servicegroups_list[srvgrp_ptr] = groupname;
                        srvgrp_ptr++;
                        logger( GM_LOG_DEBUG, "added seperate worker for servicegroup: %s\n", groupname );
                    }
                }
            }
            else if ( !strcmp( key, "hostgroups" ) || !strcmp( key, "--hostgroups" ) ) {
                char *groupname;
                while ( (groupname = strsep( &value, "," )) != NULL ) {
                    if ( strcmp( groupname, "" ) ) {
                        gearman_hostgroups_list[hostgrp_ptr] = groupname;
                        hostgrp_ptr++;
                        logger( GM_LOG_DEBUG, "added seperate worker for hostgroup: %s\n", groupname );
                    }
                }
            }

            else  {
                logger( GM_LOG_ERROR, "unknown option: %s\n", key );
            }
        }
        x++;
    }

    // did we get any server?
    if(srv_ptr == 0) {
        logger( GM_LOG_ERROR, "please specify at least one server\n" );
        exit(EXIT_FAILURE);
    }

    // nothing set by hand -> defaults
    if(set_by_hand == 0 && srvgrp_ptr == 0 && hostgrp_ptr == 0) {
        logger( GM_LOG_DEBUG, "starting client with default queues\n" );
        gearman_opt_hosts    = GM_ENABLED;
        gearman_opt_services = GM_ENABLED;
        gearman_opt_events   = GM_ENABLED;
    }

    if(srvgrp_ptr == 0 && hostgrp_ptr == 0 && gearman_opt_hosts == GM_DISABLED && gearman_opt_services == GM_DISABLED && gearman_opt_events == GM_DISABLED) {
        logger( GM_LOG_ERROR, "starting client without queues is useless\n" );
        exit(EXIT_FAILURE);
    }

    if(gearman_opt_min_worker > gearman_opt_max_worker)
        gearman_opt_min_worker = gearman_opt_max_worker;

}


/* print usage */
void print_usage() {
    printf("usage:\n");
    printf("\n");
    printf("worker [ --debug=<lvl>         ]\n");
    printf("       [ --debug-result        ]\n");
    printf("       [ --help|-h             ]\n");
    printf("\n");
    printf("       [ --server=<server>     ]\n");
    printf("\n");
    printf("       [ --hosts               ]\n");
    printf("       [ --services            ]\n");
    printf("       [ --events              ]\n");
    printf("       [ --hostgroup=<name>    ]\n");
    printf("       [ --servicegroup=<name> ]\n");
    printf("\n");
    printf("       [ --min-worker=<nr>     ]\n");
    printf("       [ --max-worker=<nr>     ]\n");
    printf("\n");
    printf("       [ --timeout             ]\n");
    printf("\n");
    printf("\n");

    exit( EXIT_SUCCESS );
}


/* increase the number of jobs */
void increase_jobs(int sig) {
    logger( GM_LOG_TRACE, "increase_jobs(%i)\n", sig);
    current_number_of_jobs++;
}


/* decrease the number of jobs */
void decrease_jobs(int sig) {
    logger( GM_LOG_TRACE, "decrease_jobs(%i)\n", sig);
    current_number_of_jobs--;
}


/* set new number of workers */
int adjust_number_of_worker(int min, int max, int cur_workers, int cur_jobs) {
    int perc_running = (int)cur_jobs*100/cur_workers;
    int idle         = (int)cur_workers - cur_jobs;
    logger( GM_LOG_ERROR, "adjust_number_of_worker(min %d, max %d, worker %d, jobs %d) = %d%% running\n", min, max, cur_workers, cur_jobs, perc_running);
    int target = min;

    if(cur_workers == max)
        return max;

    // > 90% workers running
    if(cur_jobs > 0 && ( perc_running > 90 || idle <= 2 )) {
        // increase target number by 2
        logger( GM_LOG_TRACE, "starting 2 new workers\n");
        target = cur_workers + 2;
    }

    // dont go over the top
    if(target > max) { target = max; }

    return target;
}
