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
#include "gearman_top.h"
#include "utils.h"
#include "gearman_utils.h"

int opt_verbose    = GM_DISABLED;
int opt_quiet      = GM_DISABLED;
int opt_interval   = 1;

char * server_list[GM_LISTSIZE];
int server_list_num = 0;


/* work starts here */
int main (int argc, char **argv) {
    int opt;
    WINDOW *w;

    mod_gm_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);

    /*
     * and parse command line
     */
    while((opt = getopt(argc, argv, "qvVhH:s:i:")) != -1) {
        switch(opt) {
            case 'h':   print_usage();
                        break;
            case 'v':   opt_verbose++;
                        break;
            case 'V':   print_version();
                        break;
            case 's':
            case 'H':   server_list[server_list_num++] = optarg;
                        break;
            case 'q':   opt_quiet = GM_ENABLED;
                        break;
            case 'i':   opt_interval = atoi(optarg);
                        break;
            case '?':   printf("Error - No such option: `%c'\n\n", optopt);
                        print_usage();
                        break;
        }
    }
    mod_gm_opt->debug_level = opt_verbose;
    server_list[server_list_num] = NULL;

    if(server_list_num == 0) {
        printf("Error - no hostname given\n\n");
        print_usage();
    }

    if(opt_interval <= 0)
        opt_interval = 1;

    signal(SIGINT, clean_exit);
    signal(SIGTERM,clean_exit);

    /* init curses */
    w = initscr();
    cbreak();
    nodelay(w, TRUE);
    noecho();

    /* print stats in a loop */
    while(1) {
        if(getch() == 'q')
            clean_exit(0);
        print_stats(server_list[0]);
        sleep(opt_interval);
    }

    clean_exit(0);
}


/* clean exit */
void clean_exit(int sig) {
    logger( GM_LOG_DEBUG, "clean_exit(%i)\n", sig );

    endwin();
    exit( EXIT_SUCCESS );
}

/* print version */
void print_version() {
    printf("gearman_top: version %s\n", GM_VERSION );
    printf("\n");
    exit( EXIT_SUCCESS );
}


/* print usage */
void print_usage() {
    printf("usage:\n");
    printf("\n");
    printf("gearman_top   [ -H <hostname>                  ]\n");
    printf("              [ -i <sec>       seconds         ]\n");
    printf("              [ -q             quiet mode      ]\n");
    printf("\n");
    printf("              [ -h             print help      ]\n");
    printf("              [ -v             verbose output  ]\n");
    printf("              [ -V             print version   ]\n");
    printf("\n");

    exit( EXIT_SUCCESS );
}


/* print stats */
void print_stats(char * hostname) {
    char * server = NULL;
    char * port_c = NULL;
    char * message = NULL;
    char * version = NULL;
    char cur_time[GM_BUFFERSIZE];
    mod_gm_server_status_t *stats;
    int port = GM_SERVER_DEFAULT_PORT;
    int rc;
    int x;
    struct tm now;
    time_t t;

    logger( GM_LOG_DEBUG, "print_stats()\n");

    server = strsep(&hostname, ":");
    port_c = strsep(&hostname, "\x0");
    if(port_c != NULL)
        port = atoi(port_c);

    /* get stats */
    stats = malloc(sizeof(mod_gm_server_status_t));
    stats->function_num = 0;
    rc = get_gearman_server_data(stats, &message, &version, server, port);

    t   = time(NULL);
    now = *(localtime(&t));
    strftime(cur_time, sizeof(cur_time), "%Y-%m-%d %H:%M:%S", &now );

    erase(); /* clear screen */
    printw("%s  -  %s:%i ", cur_time, server, port );
    if(version != NULL && strcmp(version, "") != 0)
        printw("  -  v%s", version );
    printw("\n\n");

    if( rc == STATE_OK ) {
        printw(" Queue Name           | Worker Available   | Jobs Waiting  | Jobs Running\n");
        printw("--------------------------------------------------------------------------\n");
        for(x=0; x<stats->function_num;x++) {
            printw(" %-20s |      %10i    |  %10i   |  %10i\n", stats->function[x]->queue, stats->function[x]->worker, stats->function[x]->waiting, stats->function[x]->running);
        }
        if(stats->function_num == 0)
            printw("                             no queues found\n");
        printw("--------------------------------------------------------------------------\n");
    }
    else {
        printw(" %s\n", message);
    }
    refresh();

    free(message);
    free(version);
    free_mod_gm_status_server(stats);
    return;
}
