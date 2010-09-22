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

#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>


/* function status structure */
typedef struct mod_gm_status_function {
    char         * queue;
    int            total;
    int            running;
    int            waiting;
    int            worker;
} mod_gm_status_function_t;

/* worker status structure */
typedef struct mod_gm_status_worker {
    int            fd;
    char         * ip;
    char         * id;
    char         * function[GM_LISTSIZE];
} mod_gm_status_worker_t;


/* server status structure */
typedef struct mod_gm_status_server {
    mod_gm_status_worker_t    * worker[GM_LISTSIZE];
    int                         worker_num;
    mod_gm_status_function_t  * function[GM_LISTSIZE];
    int                         function_num;
} mod_gm_server_status_t;


int get_gearman_server_data(mod_gm_server_status_t *stats, char ** message, char **version, char * hostname, int port);
