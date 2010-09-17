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
#include "gearman_utils.h"

int opt_verbose    =   0;
int opt_timeout    =  10;
int opt_warning    =  10;
int opt_critical   = 100;
char * opt_server  = NULL;
char * opt_queue   = NULL;
in_port_t opt_port = GM_SERVER_DEFAULT_PORT;


/* work starts here */
int main (int argc, char **argv) {

    /*
     * and parse command line
     */

    int opt;
    while((opt = getopt(argc, argv, "hvH:t:w:c:q:")) != -1) {
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

    /* set alarm */
    signal(SIGALRM, alarm_sighandler);
    alarm(opt_timeout);

    /* connect to gearman server */
    int result = check_server(opt_server, opt_port);

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
    printf("              [ -v           verbose output  ]\n");
    printf("              [ -h           print help      ]\n");
    printf("\n");
    printf("perfdata format:\n");
    printf(" |queue=waiting jobs;running jobs;worker;jobs warning;jobs critical\n");
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

    if( rc == STATE_OK ) {
        for(x=0; x<stats->function_num;x++) {
            if(opt_queue != NULL && strcmp(opt_queue, stats->function[x]->queue))
                continue;
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
        printf("OK - ");
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