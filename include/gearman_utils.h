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
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <assert.h>
#include <netinet/in.h>
#include "libgearman-1.0/gearman.h"
#include "openssl/evp.h"

typedef void*( mod_gm_worker_fn)(gearman_job_st *job, void *context, size_t *result_size, gearman_return_t *ret_ptr);

gearman_client_st * create_client( gm_server_t * server_list[GM_LISTSIZE]);
gearman_client_st * create_client_blocking( gm_server_t * server_list[GM_LISTSIZE]);
gearman_worker_st * create_worker(gm_server_t * server_list[GM_LISTSIZE]);
int add_job_to_queue(gearman_client_st **client, gm_server_t * server_list[GM_LISTSIZE], char * queue, char * uniq, char * data, int priority, int retries, int transport_mode, EVP_CIPHER_CTX * ctx, int async, int stats_log_interval);
int worker_add_function( gearman_worker_st * worker, char * queue, gearman_worker_fn *function);
void gm_free_client(gearman_client_st **client);
void gm_free_worker(gearman_worker_st **worker);

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
    mod_gm_status_worker_t      worker[GM_LISTSIZE];   /**< list of worker */
    int                         worker_num;            /**< number of worker */
    mod_gm_status_function_t    function[GM_LISTSIZE]; /**< number of functions */
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
 * send2gearmandadmin
 *
 * send command via gearman admin protocol
 *
 * @param[in] cmd - cmd to send
 * @param[in] hostname - hostname to connect to
 * @param[in] port - port to connect
 * @param[out] output - result from gearmand
 * @param[out] error - error message
 *
 * @return true on success
 */

int send2gearmandadmin(char * cmd, char * hostnam, int port, char ** output, char ** error);


/**
 * gm_net_connect
 *
 * open socket to ipv4 or ipv6 address
 *
 * @param[in] hostname - hostname to connect to
 * @param[in] port - port to connect
 * @param[out] socket - result socket
 * @param[out] error - error message
 *
 * @return true on success
 */
int gm_net_connect (const char *host_name, int port, int *sd, char ** error);

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
