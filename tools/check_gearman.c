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
#include "check_gearman.h"
#include "utils.h"
#include "gearman.h"
#include "gearman_utils.h"

int opt_verbose    =   0;
int opt_timeout    =  10;
int opt_warning    =  10;
int opt_critical   = 100;
char * opt_server  = NULL;
char * opt_queue   = NULL;
char * opt_send    = NULL;
char * opt_expect  = NULL;
in_port_t opt_port = GM_SERVER_DEFAULT_PORT;

gearman_client_st client;


/* work starts here */
int main (int argc, char **argv) {

    /*
     * and parse command line
     */

    int opt;
    while((opt = getopt(argc, argv, "hvH:t:w:c:q:s:e:p:")) != -1) {
        switch(opt) {
            case 'h':   print_usage();
                        exit( STATE_UNKNOWN );
            case 'v':   opt_verbose++;
                        break;
            case 't':   opt_timeout = atoi(optarg);
                        break;
            case 'w':   opt_warning = atoi(optarg);
                        break;
            case 'c':   opt_critical = atoi(optarg);
                        break;
            case 'H':   opt_server = optarg;
                        break;
            case 's':   opt_send = optarg;
                        break;
            case 'e':   opt_expect = optarg;
                        break;
            case 'q':   opt_queue = optarg;
                        break;
            case 'p':   opt_port = atoi(optarg);
                        break;
            case '?':   printf("Error - No such option: `%c'\n\n", optopt);
                        print_usage();
                        break;
        }
    }

    if(opt_server == NULL) {
        printf("Error - no hostname given\n\n");
        print_usage();
    }

    if(opt_send != NULL && opt_queue == NULL) {
        printf("Error - need queue (-q) when sending job\n\n");
        print_usage();
    }

    /* set alarm signal handler */
    signal(SIGALRM, alarm_sighandler);

    int result;
    if(opt_send != NULL ) {
        alarm(opt_timeout+1);
        result = check_worker(opt_server, opt_port, opt_queue, opt_send, opt_expect);
    }
    else {
        /* get gearman server statistics */
        alarm(opt_timeout);
        result = check_server(opt_server, opt_port);
    }
    alarm(0);


    exit( result );
}


/* print usage */
void print_usage() {
    printf("usage:\n");
    printf("\n");
    printf("check_gearman [ -H=<hostname>                ]\n");
    printf("              [ -p=<port>                    ]\n");
    printf("              [ -t=<timeout>                 ]\n");
    printf("              [ -w=<jobs warning level>      ]\n");
    printf("              [ -c=<jobs critical level>     ]\n");
    printf("              [ -q=<queue>                   ]\n");
    printf("\n");
    printf("\n");
    printf("to send a test job:\n");
    printf("              [ -s=<send text>               ]\n");
    printf("              [ -e=<expect text>             ]\n");
    printf("\n");
    printf("              [ -v           verbose output  ]\n");
    printf("              [ -h           print help      ]\n");
    printf("\n");
    printf("perfdata format when checking job server:\n");
    printf(" |queue=waiting jobs;running jobs;worker;jobs warning;jobs critical\n");
    printf("\n");
    printf("perfdata format when checking mod gearman worker:\n");
    printf(" |worker=10 running=1 total_jobs_done=1508\n");
    printf("\n");
    printf("Note: Job thresholds are per queue not totals.\n");
    printf("\n");
    printf("Examples:\n");
    printf("\n");
    printf("Check job server:\n");
    printf("\n");
    printf("%%>./check_gearman -H localhost -t 20\n");
    printf("check_gearman OK - 6 jobs running and 0 jobs waiting.|check_results=0;0;1;10;100 host=0;0;9;10;100\n");
    printf("\n");
    printf("Check worker:\n");
    printf("\n");
    printf("%%> ./check_gearman -H <job server hostname> -q worker_<worker hostname> -t 10 -s check\n");
    printf("check_gearman OK - localhost has 10 worker and is working on 1 jobs|worker=10 running=1 total_jobs_done=1508\n");
    printf("\n");

    exit( STATE_UNKNOWN );
}


/* called when check runs into timeout */
void alarm_sighandler(int sig) {
    logger( GM_LOG_TRACE, "alarm_sighandler(%i)\n", sig );

    printf("timeout while connecting to %s:%i\n", opt_server, opt_port);

    exit( STATE_CRITICAL );
}


/* check gearman server */
int check_server(char * hostname, int port) {
    mod_gm_server_status_t *stats;
    int x;
    stats = malloc(sizeof(mod_gm_server_status_t));
    char * message = NULL;
    int rc = get_gearman_server_data(stats, &message, hostname, port);
    int total_running = 0;
    int total_waiting = 0;
    if( rc == STATE_OK ) {
        for(x=0; x<stats->function_num;x++) {
            if(opt_queue != NULL && strcmp(opt_queue, stats->function[x]->queue))
                continue;
            total_running += stats->function[x]->running;
            total_waiting += stats->function[x]->waiting;
            if(stats->function[x]->waiting > 0 && stats->function[x]->worker == 0) {
                rc = STATE_CRITICAL;
                char * buf = malloc(GM_BUFFERSIZE);
                snprintf(buf, GM_BUFFERSIZE, "Queue %s has %i job%s without any worker. ", stats->function[x]->queue, stats->function[x]->waiting, stats->function[x]->waiting > 1 ? "s":"" );
                strncat(message, buf, GM_BUFFERSIZE);
            }
            else if(stats->function[x]->waiting >= opt_critical) {
                rc = STATE_CRITICAL;
                char * buf = malloc(GM_BUFFERSIZE);
                snprintf(buf, GM_BUFFERSIZE, "Queue %s has %i waiting job%s. ", stats->function[x]->queue, stats->function[x]->waiting, stats->function[x]->waiting > 1 ? "s":"" );
                strncat(message, buf, GM_BUFFERSIZE);
            }
            else if(stats->function[x]->waiting >= opt_warning) {
                rc = STATE_WARNING;
                char * buf = malloc(GM_BUFFERSIZE);
                snprintf(buf, GM_BUFFERSIZE, "Queue %s has %i waiting job%s. ", stats->function[x]->queue, stats->function[x]->waiting, stats->function[x]->waiting > 1 ? "s":"" );
                strncat(message, buf, GM_BUFFERSIZE);
            }
        }
    }

    /* print plugin name and state */
    printf("%s ", PLUGIN_NAME);
    if(rc == STATE_OK)
        printf("OK - %i job%s running and %i job%s waiting.", total_running, total_running==1?"":"s", total_waiting, total_waiting==1?"":"s");
    if(rc == STATE_WARNING)
        printf("WARNING - ");
    if(rc == STATE_CRITICAL)
        printf("CRITICAL - ");
    if(rc == STATE_UNKNOWN)
        printf("UNKNOWN - ");
    printf("%s", message);

    /* print performance data */
    if(stats->function_num > 0) {
        printf("|");
        for(x=0; x<stats->function_num;x++) {
            if(opt_queue != NULL && strcmp(opt_queue, stats->function[x]->queue))
                continue;
            printf( "%s=%i;%i;%i;%i;%i ", stats->function[x]->queue, stats->function[x]->waiting, stats->function[x]->running, stats->function[x]->worker, opt_warning, opt_critical);
        }
    }
    printf("\n");

    free(message);
    free(stats);
    return( rc );
}


/* send job to worker and check result */
int check_worker(char * hostname, int port, char * queue, char * send, char * expect) {

    char * server_list[2];
    char server[256];
    snprintf(server, 256, "%s:%i", hostname, port);
    server_list[0] = server;
    server_list[1] = NULL;

    /* create client */
    if ( create_client( server_list, &client ) != GM_OK ) {
        printf("%s UNKNOWN - cannot start client for %s\n", PLUGIN_NAME, server);
        return( STATE_UNKNOWN );
    }
    gearman_client_set_timeout(&client, opt_timeout*1000);

    gearman_return_t ret;
    char * result;
    size_t result_size;
    while (1) {
        result= (char *)gearman_client_do_high( &client,
                                                queue,
                                                "check",
                                                (void *)send,
                                                (size_t)strlen(send),
                                                &result_size,
                                                &ret);
        if (ret == GEARMAN_WORK_DATA) {
            free(result);
            continue;
        }
        else if (ret == GEARMAN_WORK_STATUS) {
            continue;
        }
        else if (ret == GEARMAN_SUCCESS) {
            gearman_client_free(&client);
        }
        else if (ret == GEARMAN_WORK_FAIL) {
            printf("%s CRITICAL - Job failed\n", PLUGIN_NAME);
            gearman_client_free(&client);
            return( STATE_CRITICAL );
        }
        else {
            printf("%s CRITICAL - Job failed: %s\n", PLUGIN_NAME, gearman_client_error(&client));
            gearman_client_free(&client);
            return( STATE_CRITICAL );
        }
        break;
    }

    if( expect != NULL ) {
        if( strstr(result, expect) != NULL) {
            printf("%s OK - send worker %s: '%s' response: '%s'\n", PLUGIN_NAME, server, send, result);
            return( STATE_OK );
        }
        else {
            printf("%s CRITICAL - send worker %s: '%s' response: '%s', expected '%s'\n", PLUGIN_NAME, server, send, result, expect);
            return( STATE_CRITICAL );
        }
    }

    printf("%s OK - %s\n", PLUGIN_NAME, result );
    return( STATE_OK );
}

