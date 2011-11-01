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

/** @file
 *  @brief gearman specific utils
 *
 *  @{
 */

#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>

/** function status structure */
typedef struct mod_gm_status_function {
    char         * queue;         /**< name of the queue (function name) */
    int            total;         /**< total number of workers */
    int            running;       /**< number of running worker */
    int            waiting;       /**< number of waiting jobs */
    int            worker;        /**< number of workers */
} mod_gm_status_function_t;

/** worker status structure
 *
 * not used at the moment
 */
typedef struct mod_gm_status_worker {
    int            fd;                    /**< file descriptor */
    char         * ip;                    /**< ip from this worker */
    char         * id;                    /**< id of the worker */
    char         * function[GM_LISTSIZE]; /**< list of functions returned from gearmand */
} mod_gm_status_worker_t;


/** server status structure */
typedef struct mod_gm_status_server {
    mod_gm_status_worker_t    * worker[GM_LISTSIZE];   /**< list of worker */
    int                         worker_num;            /**< number of worker */
    mod_gm_status_function_t  * function[GM_LISTSIZE]; /**< number of functions */
    int                         function_num;          /**< number of functions */
} mod_gm_server_status_t;

/**
 * get_gearman_server_data
 *
 * return admin statistics from gearmand
 *
 * @param[out] stats - stats structure
 * @param[out] message - info/error message
 * @param[out] version - version string from server
 * @param[in] hostname - hostname to connect to
 * @param[in] port - port to connect
 *
 * @return true on success
 */
int get_gearman_server_data(mod_gm_server_status_t *stats, char ** message, char **version, char * hostname, int port);

/**
 * free_mod_gm_status_server
 *
 * free status structure
 *
 * @param[in] stats - structure to free
 *
 * @return nothing
 */
void free_mod_gm_status_server(mod_gm_server_status_t *stats);

/**
 * struct_cmp_by_queue
 *
 * sort gearman queues by name
 *
 * @param[in] a - structure a
 * @param[in] b - structure b
 *
 * @return true on success
 */
int struct_cmp_by_queue(const void *a, const void *b);

/**
 * @}
 */

