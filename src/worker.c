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
#include "worker_logger.h"
#include "worker_client.h"

int gearman_opt_min_worker      = 3;
int gearman_opt_max_worker      = 50;

int gearman_opt_hosts           = GM_DISABLED;
int gearman_opt_services        = GM_DISABLED;
int gearman_opt_events          = GM_DISABLED;
char *gearman_hostgroups_list[GM_LISTSIZE];
char *gearman_servicegroups_list[GM_LISTSIZE];

int current_number_of_workers   = 0;

/* work starts here */
int main (int argc, char **argv) {

gearman_opt_debug_level = GM_LOG_TRACE;

    parse_arguments(argv);
    logger( GM_LOG_DEBUG, "main process started\n");

    // standalone mode?
    if(gearman_opt_min_worker == 1 && gearman_opt_max_worker == 1) {
        worker_client();
        exit( EXIT_SUCCESS );
    }

    // create initial childs
    int x;
    for(x=0; x < gearman_opt_min_worker; x++) {
        make_new_child();
    }

    // And maintain the population
    while (1) {
        // check number of workers every second
        sleep(1);

        // collect finished workers
        int status;
        while(waitpid(-1, &status, WNOHANG) > 0) {
            current_number_of_workers--;
            logger( GM_LOG_TRACE, "waitpid() %d\n", status);
        }

        for (x = current_number_of_workers; x < gearman_opt_min_worker; x++) {
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

        // do the real work
        worker_client();

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
        char * args = strdup( argv[x] );
        while ( (ptr = strsep( &args, " " )) != NULL ) {
            char *key   = str_token( &ptr, '=' );
            char *value = str_token( &ptr, 0 );

            if ( key == NULL )
                continue;

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

            if ( value == NULL )
                continue;

            if ( !strcmp( key, "debug" ) || !strcmp( key, "--debug" ) ) {
                gearman_opt_debug_level = atoi( value );
                if(gearman_opt_debug_level < 0) { gearman_opt_debug_level = 0; }
                logger( GM_LOG_DEBUG, "Setting debug level to %d\n", gearman_opt_debug_level );
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

}
