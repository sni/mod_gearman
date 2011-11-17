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

int opt_verbose         =   0;
int opt_timeout         =  10;
int opt_job_warning     =  10;
int opt_job_critical    = 100;
int opt_worker_warning  =  25;
int opt_worker_critical =  50;
char * opt_server       = NULL;
char * opt_queue        = NULL;
char * opt_send         = NULL;
char * opt_expect       = NULL;
char * opt_unique_id    = NULL;
int send_async          = 0;

char * server_list[GM_LISTSIZE];
int server_list_num = 0;

gearman_client_st client;


/* work starts here */
int main (int argc, char **argv) {
    int opt;
    int result;

    mod_gm_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);

    /*
     * and parse command line
     */
    while((opt = getopt(argc, argv, "vVhaH:t:w:c:W:C:q:s:e:p:u:")) != -1) {
        switch(opt) {
            case 'h':   print_usage();
                        break;
            case 'v':   opt_verbose++;
                        break;
            case 'V':   print_version();
                        break;
            case 't':   opt_timeout = atoi(optarg);
                        break;
            case 'w':   opt_job_warning = atoi(optarg);
                        break;
            case 'c':   opt_job_critical = atoi(optarg);
                        break;
            case 'W':   opt_worker_warning = atoi(optarg);
                        break;
            case 'C':   opt_worker_critical = atoi(optarg);
                        break;
            case 'H':   opt_server = optarg;
                        server_list[server_list_num++] = optarg;
                        break;
            case 's':   opt_send = optarg;
                        break;
            case 'a':   send_async = 1;
                        break;
            case 'e':   opt_expect = optarg;
                        break;
            case 'q':   opt_queue = optarg;
                        break;
            case 'u':   opt_unique_id = optarg;
                        break;
            case '?':   printf("Error - No such option: `%c'\n\n", optopt);
                        print_usage();
                        break;
        }
    }
    mod_gm_opt->debug_level = opt_verbose;
    mod_gm_opt->logmode     = GM_LOG_MODE_TOOLS;
    server_list[server_list_num] = NULL;

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

    if(opt_send != NULL ) {
        alarm(opt_timeout);
        result = check_worker(opt_queue, opt_send, opt_expect);
    }
    else {
        /* get gearman server statistics */
        alarm(opt_timeout);
        result = check_server(opt_server);
    }
    alarm(0);

    exit( result );
}


/* print version */
void print_version() {
    printf("check_gearman: version %s running on libgearman %s\n", GM_VERSION, gearman_version());
    printf("\n");
    exit( STATE_UNKNOWN );
}


/* print usage */
void print_usage() {
    printf("usage:\n");
    printf("\n");
    printf("check_gearman [ -H=<hostname>                ]\n");
    printf("              [ -t=<timeout>                 ]\n");
    printf("              [ -w=<jobs warning level>      ]  default: %i\n", opt_job_warning);
    printf("              [ -c=<jobs critical level>     ]  default: %i\n", opt_job_critical);
    printf("              [ -W=<worker warning level>    ]  default: %i\n", opt_worker_warning);
    printf("              [ -C=<worker critical level>   ]  default: %i\n", opt_worker_critical);
    printf("              [ -q=<queue>                   ]\n");
    printf("\n");
    printf("\n");
    printf("to send a test job:\n");
    printf("              [ -u=<unique job id>           ]  default: check\n");
    printf("              [ -s=<send text>               ]\n");
    printf("              [ -e=<expect text>             ]\n");
    printf("              [ -a           send async      ]  will ignore -e\n");
    printf("\n");
    printf("              [ -h           print help      ]\n");
    printf("              [ -v           verbose output  ]\n");
    printf("              [ -V           print version   ]\n");
    printf("\n");
    printf(" - You may set thresholds to 0 to disable them.\n");
    printf(" - Thresholds are only for server checks, worker checks are availability only\n");
    printf("\n");
    printf("perfdata format when checking job server:\n");
    printf(" |'queue waiting'=current waiting jobs;warn;crit;0 'queue running'=current running jobs 'queue worker'=current num worker;warn;crit;0\n");
    printf("\n");
    printf("Note: set your pnp RRD_STORAGE_TYPE to MULTIPLE to support changeing numbers of queues.\n");
    printf("      see http://docs.pnp4nagios.org/de/pnp-0.6/tpl_custom for detailed information\n");
    printf("\n");
    printf("perfdata format when checking mod gearman worker:\n");
    printf(" |worker=10 jobs=1508c\n");
    printf("\n");
    printf("Note: Job thresholds are per queue not totals.\n");
    printf("\n");
    printf("Examples:\n");
    printf("\n");
    printf("Check job server:\n");
    printf("\n");
    printf("%%>./check_gearman -H localhost -q host\n");
    printf("check_gearman OK - 0 jobs running and 0 jobs waiting. Version: 0.14|'host_waiting'=0;10;15;0 'host_running'=0 'host_worker'=3;5;10;0\n");
    printf("\n");
    printf("Check worker:\n");
    printf("\n");
    printf("%%> ./check_gearman -H <job server hostname> -q worker_<worker hostname> -t 10 -s check\n");
    printf("check_gearman OK - host has 5 worker and is working on 0 jobs|worker=5 jobs=96132c\n");
    printf("\n");

    exit( STATE_UNKNOWN );
}


/* called when check runs into timeout */
void alarm_sighandler(int sig) {
    gm_log( GM_LOG_TRACE, "alarm_sighandler(%i)\n", sig );

    printf("timeout while waiting for %s\n", opt_server);

    exit( STATE_CRITICAL );
}


/* check gearman server */
int check_server(char * hostname) {
    int port = GM_SERVER_DEFAULT_PORT;
    mod_gm_server_status_t *stats;
    int x;
    char * message = NULL;
    char * version = NULL;
    char * server = NULL;
    char * port_c = NULL;
    int total_running = 0;
    int total_waiting = 0;
    int checked       = 0;
    int rc;
    char *buf;

    server = strsep(&hostname, ":");
    port_c = strsep(&hostname, "\x0");
    if(port_c != NULL)
        port = atoi(port_c);

    stats = malloc(sizeof(mod_gm_server_status_t));
    rc = get_gearman_server_data(stats, &message, &version, server, port);
    if( rc == STATE_OK ) {
        for(x=0; x<stats->function_num;x++) {
            if(opt_queue != NULL && strcmp(opt_queue, stats->function[x]->queue))
                continue;
            checked++;
            total_running += stats->function[x]->running;
            total_waiting += stats->function[x]->waiting;
            if(stats->function[x]->waiting > 0 && stats->function[x]->worker == 0) {
                rc = STATE_CRITICAL;
                buf = malloc(GM_BUFFERSIZE);
                snprintf(buf, GM_BUFFERSIZE, "Queue %s has %i job%s without any worker. ", stats->function[x]->queue, stats->function[x]->waiting, stats->function[x]->waiting > 1 ? "s":"" );
                strncat(message, buf, GM_BUFFERSIZE);
            }
            else if(opt_job_critical > 0 && stats->function[x]->waiting >= opt_job_critical) {
                rc = STATE_CRITICAL;
                buf = malloc(GM_BUFFERSIZE);
                snprintf(buf, GM_BUFFERSIZE, "Queue %s has %i waiting job%s. ", stats->function[x]->queue, stats->function[x]->waiting, stats->function[x]->waiting > 1 ? "s":"" );
                strncat(message, buf, GM_BUFFERSIZE);
            }
            else if(opt_worker_critical > 0 && stats->function[x]->worker >= opt_worker_critical) {
                rc = STATE_CRITICAL;
                buf = malloc(GM_BUFFERSIZE);
                snprintf(buf, GM_BUFFERSIZE, "Queue %s has %i worker. ", stats->function[x]->queue, stats->function[x]->worker );
                strncat(message, buf, GM_BUFFERSIZE);
            }
            else if(opt_job_warning > 0 && stats->function[x]->waiting >= opt_job_warning) {
                rc = STATE_WARNING;
                buf = malloc(GM_BUFFERSIZE);
                snprintf(buf, GM_BUFFERSIZE, "Queue %s has %i waiting job%s. ", stats->function[x]->queue, stats->function[x]->waiting, stats->function[x]->waiting > 1 ? "s":"" );
                strncat(message, buf, GM_BUFFERSIZE);
            }
            else if(opt_worker_warning > 0 && stats->function[x]->worker >= opt_worker_warning) {
                rc = STATE_WARNING;
                buf = malloc(GM_BUFFERSIZE);
                snprintf(buf, GM_BUFFERSIZE, "Queue %s has %i worker. ", stats->function[x]->queue, stats->function[x]->worker );
                strncat(message, buf, GM_BUFFERSIZE);
            }
        }
    }
    if(opt_queue != NULL && checked == 0) {
        buf = malloc(GM_BUFFERSIZE);
        snprintf(buf, GM_BUFFERSIZE, "Queue %s not found", opt_queue );
        strncat(message, buf, GM_BUFFERSIZE);
        rc = STATE_WARNING;
    }

    /* print plugin name and state */
    printf("%s ", PLUGIN_NAME);
    if(rc == STATE_OK)
        printf("OK - %i job%s running and %i job%s waiting. Version: %s", total_running, total_running==1?"":"s", total_waiting, total_waiting==1?"":"s", version);
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
            printf( "'%s_waiting'=%i;%i;%i;0 '%s_running'=%i '%s_worker'=%i;%i;%i;0 ",
                      stats->function[x]->queue,
                      stats->function[x]->waiting,
                      opt_job_warning,
                      opt_job_critical,
                      stats->function[x]->queue,
                      stats->function[x]->running,
                      stats->function[x]->queue,
                      stats->function[x]->worker,
                      opt_worker_warning,
                      opt_worker_critical
                  );
        }
    }
    printf("\n");

    free(message);
    free(version);
    free_mod_gm_status_server(stats);
    return( rc );
}


/* send job to worker and check result */
int check_worker(char * queue, char * to_send, char * expect) {
    gearman_return_t ret;
    char * result;
    size_t result_size;
    char * job_handle;
    const char * unique_job_id;
    if (opt_unique_id == NULL) {
        unique_job_id = "check";
    }
    else {
        unique_job_id = opt_unique_id;
    }

    /* create client */
    if ( create_client( server_list, &client ) != GM_OK ) {
        printf("%s UNKNOWN - cannot create gearman client\n", PLUGIN_NAME);
        return( STATE_UNKNOWN );
    }
    gearman_client_set_timeout(&client, (opt_timeout-1)*1000/server_list_num);

    while (1) {
        if (send_async) {
            result = "sending background job succeded";
            job_handle = malloc(GEARMAN_JOB_HANDLE_SIZE * sizeof(char));
            ret= gearman_client_do_high_background( &client,
                                                    queue,
                                                    unique_job_id,
                                                    (void *)to_send,
                                                    (size_t)strlen(to_send),
                                                    job_handle);
            free(job_handle);
        }
        else {
            result= (char *)gearman_client_do_high( &client,
                                                    queue,
                                                    unique_job_id,
                                                    (void *)to_send,
                                                    (size_t)strlen(to_send),
                                                    &result_size,
                                                    &ret);
        }

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

    if( !send_async && expect != NULL && result != NULL ) {
        if( strstr(result, expect) != NULL) {
            printf("%s OK - send worker '%s' response: '%s'\n", PLUGIN_NAME, to_send, result);
            return( STATE_OK );
        }
        else {
            printf("%s CRITICAL - send worker: '%s' response: '%s', expected '%s'\n", PLUGIN_NAME, to_send, result, expect);
            return( STATE_CRITICAL );
        }
    }

    printf("%s OK - %s\n", PLUGIN_NAME, result );
    return( STATE_OK );
}


/* core log wrapper */
void write_core_log(char *data) {
    printf("core logger is not available for tools: %s", data);
    return;
}
