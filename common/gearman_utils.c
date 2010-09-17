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

#include "common.h"
#include "utils.h"
#include "gearman_utils.h"


/* get worker/jobs data from gearman server */
int get_gearman_server_data(mod_gm_server_status_t *stats, char ** message, char * hostname, int port) {
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    *message = malloc(GM_BUFFERSIZE);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if( sockfd < 0 ) {
        snprintf(*message, GM_BUFFERSIZE, "failed to open socket: %s\n", strerror(errno));
        return( STATE_CRITICAL );
    }

    server = gethostbyname(hostname);
    if( server == NULL ) {
        snprintf(*message, GM_BUFFERSIZE, "failed to resolve %s\n", hostname);
        return( STATE_CRITICAL );
    }
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(port);
    if (connect(sockfd,(const struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
        snprintf(*message, GM_BUFFERSIZE, "failed to connect to %s:%i - %s\n", hostname, (int)port, strerror(errno));
        return( STATE_CRITICAL );
    }

    char * cmd = "status\n";
    int n = write(sockfd,cmd,strlen(cmd));
    if (n < 0) {
        snprintf(*message, GM_BUFFERSIZE, "failed to send to %s:%i - %s\n", hostname, (int)port, strerror(errno));
        return( STATE_CRITICAL );
    }

    char buf[GM_BUFFERSIZE];
    n = read( sockfd, buf, GM_BUFFERSIZE-1 );
    buf[n] = '\x0';
    if (n < 0) {
        snprintf(*message, GM_BUFFERSIZE, "error reading from %s:%i - %s\n", hostname, (int)port, strerror(errno));
        return( STATE_CRITICAL );
    }

    char * line;
    char * output = strdup(buf);
    while ( (line = strsep( &output, "\n" )) != NULL ) {
        logger( GM_LOG_TRACE, "%s\n", line );
        if(!strcmp( line, "."))
            break;
        char * name    = strsep(&line, "\t");
        char * total   = strsep(&line, "\t");
        char * running = strsep(&line, "\t");
        char * worker  = strsep(&line, "\x0");
        mod_gm_status_function_t *func = malloc(sizeof(mod_gm_status_function_t));
        func->queue   = name;
        func->running = atoi(running);
        func->total   = atoi(total);
        func->worker  = atoi(worker);
        func->waiting = func->total - func->running;
        stats->function[stats->function_num++] = func;
        logger( GM_LOG_DEBUG, "%i: name:%-20s worker:%-5i waiting:%-5i running:%-5i\n", stats->function_num, func->queue, func->worker, func->waiting, func->running );
    }

    return( STATE_OK );
}