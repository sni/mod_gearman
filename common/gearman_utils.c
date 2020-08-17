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

int mod_gm_con_errors = 0;
struct timeval mod_gm_error_time;
extern mod_gm_opt_t *mod_gm_opt;

/* create the gearman worker */
int create_worker( gm_server_t * server_list[GM_LISTSIZE], gearman_worker_st *worker ) {
    int x = 0;

    gearman_return_t ret;
    signal(SIGPIPE, SIG_IGN);

    gearman_worker_create( worker );
    if ( worker == NULL ) {
        gm_log( GM_LOG_ERROR, "Memory allocation failure on worker creation\n" );
        return GM_ERROR;
    }

    while ( server_list[x] != NULL ) {
        ret = gearman_worker_add_server( worker, server_list[x]->host, server_list[x]->port );
        if ( ret != GEARMAN_SUCCESS ) {
            gm_log( GM_LOG_ERROR, "worker error: %s\n", gearman_worker_error( worker ) );
            return GM_ERROR;
        }
        x++;
    }
    assert(x != 0);

    return GM_OK;
}


/* register function on worker */
int worker_add_function( gearman_worker_st * worker, char * queue, gearman_worker_fn *function) {
    gearman_return_t ret;
    ret = gearman_worker_add_function( worker, queue, 0, function, NULL );
    if ( ret != GEARMAN_SUCCESS ) {
        gm_log( GM_LOG_ERROR, "worker error: %s\n", gearman_worker_error( worker ) );
        return GM_ERROR;
    }

    return GM_OK;
}


/* create the gearman client */
int create_client( gm_server_t * server_list[GM_LISTSIZE], gearman_client_st *client ) {
    gearman_return_t ret;
    int x = 0;

    gm_log( GM_LOG_TRACE, "create_client()\n" );

    signal(SIGPIPE, SIG_IGN);

    client = gearman_client_create(client);
    if ( client == NULL ) {
        gm_log( GM_LOG_ERROR, "Memory allocation failure on client creation\n" );
        return GM_ERROR;
    }

    while ( server_list[x] != NULL ) {
        ret = gearman_client_add_server( client, server_list[x]->host, server_list[x]->port );
        if ( ret != GEARMAN_SUCCESS ) {
            gm_log( GM_LOG_ERROR, "client error: %s\n", gearman_client_error( client ) );
            return GM_ERROR;
        }
        x++;
    }
    assert(x != 0);

    gearman_client_set_timeout( client, mod_gm_opt->gearman_connection_timeout );

    return GM_OK;
}


/* create a task and send it */
int add_job_to_queue( gearman_client_st *client, gm_server_t * server_list[GM_LISTSIZE], char * queue, char * uniq, char * data, int priority, int retries, int transport_mode) {
    gearman_job_handle_t job_handle;
    gearman_return_t rc;
    char * crypted_data;
    int size, free_uniq, ret;
    struct timeval now;

    /* check too long queue names */
    if(strlen(queue) > GEARMAN_FUNCTION_MAX_SIZE - 1) {
        gm_log( GM_LOG_ERROR, "queue name too long: '%s'\n", queue );
        return GM_ERROR;
    }

    /* uniq identifier must not exceed certain size */
    if(uniq != NULL && strlen(uniq) > GEARMAN_MAX_UNIQUE_SIZE - 1) {
        gm_log( GM_LOG_ERROR, "unique name too long: '%s'\n", uniq );
        return GM_ERROR;
    }

    signal(SIGPIPE, SIG_IGN);

    gm_log( GM_LOG_TRACE, "add_job_to_queue(%s, %s, %d, %d, %d)\n", queue, uniq, priority, retries, transport_mode);
    gm_log( GM_LOG_TRACE, "%d --->%s<---\n", strlen(data), data );

    size = mod_gm_encrypt(&crypted_data, data, transport_mode);
    gm_log( GM_LOG_TRACE, "%d +++>\n%s\n<+++\n", size, crypted_data );

    if( priority == GM_JOB_PRIO_LOW ) {
        rc = gearman_client_do_low_background(client, queue, uniq, ( void * )crypted_data, ( size_t )size, job_handle);
    }
    else if( priority == GM_JOB_PRIO_NORMAL ) {
        rc = gearman_client_do_background(client, queue, uniq, ( void * )crypted_data, ( size_t )size, job_handle);
    }
    else if( priority == GM_JOB_PRIO_HIGH ) {
        rc = gearman_client_do_high_background(client, queue, uniq, ( void * )crypted_data, ( size_t )size, job_handle);
    }
    else {
        gm_log( GM_LOG_ERROR, "add_job_to_queue() wrong priority: %d\n", priority );
    }
    free(crypted_data);

    if(!gearman_success(rc)) {
        /* log the error */
        if(retries == 0) {
            gettimeofday(&now,NULL);
            /* only log the first error, otherwise we would fill the log very quickly */
            if( mod_gm_con_errors == 0 ) {
                gettimeofday(&mod_gm_error_time,NULL);
                gm_log( GM_LOG_ERROR, "sending job to gearmand failed: %s\n", gearman_client_error(client) );
            }
            /* or every minute to give an update */
            else if( now.tv_sec >= mod_gm_error_time.tv_sec + 60) {
                gettimeofday(&mod_gm_error_time,NULL);
                gm_log( GM_LOG_ERROR, "sending job to gearmand failed: %s (%i lost jobs so far)\n", gearman_client_error(client), mod_gm_con_errors );
            }
            mod_gm_con_errors++;
        }

        /* recreate client, otherwise gearman sigsegvs */
        gearman_client_free( client );
        create_client( server_list, client );

        /* retry as long as we have retries */
        if(retries > 0) {
            retries--;
            gm_log( GM_LOG_TRACE, "add_job_to_queue() retrying... %d\n", retries );
            ret = add_job_to_queue( client, server_list, queue, uniq, data, priority, retries, transport_mode);
            return(ret);
        }
        /* no more retries... */
        else {
            gm_log(GM_LOG_TRACE, "add_job_to_queue() finished with errors: %d\n", ret);
            return GM_ERROR;
        }
    }

    /* reset error counter */
    mod_gm_con_errors = 0;

    gm_log( GM_LOG_TRACE, "add_job_to_queue() finished successfully\n");
    return GM_OK;
}


void *dummy( gearman_job_st *job, void *context, size_t *result_size, gearman_return_t *ret_ptr ) {

    /* avoid "unused parameter" warning */
    job         = job;
    context     = context;
    result_size = 0;
    ret_ptr     = ret_ptr;
    result_size = result_size;

    return NULL;
}


/* free client structure */
void free_client(gearman_client_st *client) {
    gearman_client_free( client );
    return;
}


/* free worker structure */
void free_worker(gearman_worker_st *worker) {
    gearman_worker_free( worker );
    return;
}

/* get worker/jobs data from gearman server */
int get_gearman_server_data(mod_gm_server_status_t *stats, char ** message, char ** version, char * hostnam, int port) {
    int rc;
    char *total, *running, *worker, *output, *output_c, *line, *name;
    mod_gm_status_function_t *func;

    *version  = gm_malloc(GM_BUFFERSIZE);
    snprintf(*version,  GM_BUFFERSIZE, "%s", "" );

    rc = send2gearmandadmin("status\nversion\n", hostnam, port, &output, message);
    if(rc != STATE_OK) {
        if(output != NULL)
            free(output);
        return rc;
    }

    output_c = output;
    while ( (line = strsep( &output, "\n" )) != NULL ) {
        gm_log( GM_LOG_TRACE, "%s\n", line );
        if(!strcmp( line, ".")) {
            if((line = strsep( &output, "\n" )) != NULL) {
                gm_log( GM_LOG_TRACE, "%s\n", line );
                if(line[0] == 'O') {
                    strncpy(*version, line+3, 10);
                } else {
                    snprintf(*version, GM_BUFFERSIZE, "%s", line);
                }
                gm_log( GM_LOG_TRACE, "extracted version: '%s'\n", *version );
            }

            /* sort our array by queue name */
            qsort(stats->function, stats->function_num, sizeof(mod_gm_status_function_t*), struct_cmp_by_queue);

            free(output_c);
            return( STATE_OK );
        }
        name = strsep(&line, "\t");
        if(name == NULL)
            break;
        total   = strsep(&line, "\t");
        if(total == NULL)
            break;
        running = strsep(&line, "\t");
        if(running == NULL)
            break;
        worker  = strsep(&line, "\x0");
        if(worker == NULL)
            break;
        func = gm_malloc(sizeof(mod_gm_status_function_t));
        func->queue   = gm_strdup(name);
        func->running = atoi(running);
        func->total   = atoi(total);
        func->worker  = atoi(worker);
        func->waiting = func->total - func->running;

        /* skip the dummy queue if its empty */
        if(!strcmp( name, "dummy") && func->total == 0) {
            free(func->queue);
            free(func);
            continue;
        }

        stats->function[stats->function_num++] = func;
        gm_log( GM_LOG_DEBUG, "%i: name:%-20s worker:%-5i waiting:%-5i running:%-5i\n", stats->function_num, func->queue, func->worker, func->waiting, func->running );
    }

    snprintf(*message, GM_BUFFERSIZE, "got no valid data from %s:%i\n", hostnam, (int)port);
    free(output_c);
    return(rc);
}


/* send gearman admin */
int send2gearmandadmin(char * cmd, char * hostnam, int port, char ** output, char ** error) {
    int sockfd, n;
    char buf[GM_BUFFERSIZE];

    *error  = gm_malloc(GM_BUFFERSIZE);
    snprintf(*error,  GM_BUFFERSIZE, "%s", "" );
    *output = gm_malloc(GM_BUFFERSIZE);
    snprintf(*output,  GM_BUFFERSIZE, "%s", "" );

    if(gm_net_connect(hostnam, port, &sockfd, error) != GM_OK) {
        return(STATE_CRITICAL);
    }

    gm_log( GM_LOG_TRACE, "sending '%s' to %s on port %i\n", cmd, hostnam, port );
    n = write(sockfd,cmd,strlen(cmd));
    if (n < 0) {
        snprintf(*error, GM_BUFFERSIZE, "failed to send to %s:%i - %s\n", hostnam, (int)port, strerror(errno));
        close(sockfd);
        return( STATE_CRITICAL );
    }

    n = read( sockfd, buf, GM_BUFFERSIZE-1 );
    if (n < 0) {
        snprintf(*error, GM_BUFFERSIZE, "error reading from %s:%i - %s\n", hostnam, (int)port, strerror(errno));
        close(sockfd);
        return( STATE_CRITICAL );
    }
    buf[n] = '\x0';
    free(*output);
    gm_log( GM_LOG_TRACE, "got answer:\n%s\n", buf);
    *output = gm_strdup(buf);
    close(sockfd);

    return( STATE_OK );
}

/* opens a tcp connection to a remote host */
int gm_net_connect (const char *host_name, int port, int *sd, char ** error) {
    struct addrinfo hints;
    struct addrinfo *r, *res;
    char port_str[6], host[GM_MAX_HOST_ADDRESS_LENGTH];
    size_t len;
    int result;
    int was_refused = FALSE;

    memset (&hints, 0, sizeof (hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_socktype = SOCK_STREAM;

    len = strlen (host_name);
    /* check for an [IPv6] address (and strip the brackets) */
    if (len >= 2 && host_name[0] == '[' && host_name[len - 1] == ']') {
        host_name++;
        len -= 2;
    }
    if (len >= sizeof(host))
        return GM_ERROR;
    memcpy (host, host_name, len);
    host[len] = '\0';
    snprintf (port_str, sizeof (port_str), "%d", port);
    result = getaddrinfo (host, port_str, &hints, &res);

    if (result != 0) {
        snprintf(*error, GM_BUFFERSIZE, "%s\n", gai_strerror (result));
        return GM_ERROR;
    }

    r = res;
    while (r) {
        /* attempt to create a socket */
        *sd = socket (r->ai_family, SOCK_STREAM, r->ai_protocol);

        if (*sd < 0) {
            snprintf(*error, GM_BUFFERSIZE, "%s\n", "Socket creation failed");
            freeaddrinfo (r);
            return GM_ERROR;
        }

        /* attempt to open a connection */
        result = connect (*sd, r->ai_addr, r->ai_addrlen);

        if (result == 0) {
            was_refused = FALSE;
            break;
        }

        if (result < 0) {
            switch (errno) {
            case ECONNREFUSED:
                was_refused = TRUE;
                break;
            }
        }

        close (*sd);
        r = r->ai_next;
    }
    freeaddrinfo (res);

    if (result == 0)
        return GM_OK;
    else if (was_refused) {
        snprintf(*error, GM_BUFFERSIZE, "failed to connect to address %s and port %d: %s\n", host_name, port, strerror(errno));
        return GM_ERROR;
    }
    else {
        snprintf(*error, GM_BUFFERSIZE, "failed to connect to address %s and port %d: %s\n", host_name, port, strerror(errno));
        return GM_ERROR;
    }
}


/* free a status structure */
void free_mod_gm_status_server(mod_gm_server_status_t *stats) {
    int x;

    for(x=0; x<stats->function_num;x++) {
        free(stats->function[x]->queue);
        free(stats->function[x]);
    }

    for(x=0; x<stats->worker_num;x++) {
        free(stats->worker[x]->ip);
        free(stats->worker[x]->id);
        free(stats->worker[x]);
    }

    free(stats);
}


/* qsort struct comparision function for queue name */
int struct_cmp_by_queue(const void *a, const void *b) {
    mod_gm_status_function_t **pa = (mod_gm_status_function_t **)a;
    mod_gm_status_function_t *ia  = (mod_gm_status_function_t *)*pa;

    mod_gm_status_function_t **pb = (mod_gm_status_function_t **)b;
    mod_gm_status_function_t *ib  = (mod_gm_status_function_t *)*pb;

    return strcmp(ia->queue, ib->queue);
}
