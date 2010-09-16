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
#include "send_gearman.h"
#include "utils.h"
#include "worker_logger.h"
#include "gearman.h"

gearman_client_st client;

/* work starts here */
int main (int argc, char **argv) {

    /*
     * allocate options structure
     * and parse command line
     */
    if(parse_arguments(argc, argv) != GM_OK) {
        exit( EXIT_FAILURE );
    }

    /* init crypto functions */
    if(mod_gm_opt->encryption == GM_ENABLED) {
        if(mod_gm_opt->crypt_key == NULL) {
            logger( GM_LOG_ERROR, "no encryption key provided, please use --key=... or keyfile=...\n");
            exit( EXIT_FAILURE );
        }
        mod_gm_crypt_init(mod_gm_opt->crypt_key);
    } else {
        mod_gm_opt->transportmode = GM_ENCODE_ONLY;
    }

    /* create client */
    if ( create_client( mod_gm_opt->server_list, &client ) != GM_OK ) {
        logger( GM_LOG_ERROR, "cannot start client\n" );
        exit( EXIT_FAILURE );
    }

    /* send result message */
    int rc = send_result();

    gearman_client_free( &client );
    mod_gm_free_opt(mod_gm_opt);
    exit( rc );
}


/* parse command line arguments */
int parse_arguments(int argc, char **argv) {
    int i;
    int errors = 0;
    mod_gm_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);
    for(i=1;i<argc;i++) {
        char * arg   = strdup( argv[i] );
        char * arg_c = arg;
        lc(arg);
        if ( !strcmp( arg, "help" ) || !strcmp( arg, "--help" )  || !strcmp( arg, "-h" ) ) {
            print_usage();
        }
        if(parse_args_line(mod_gm_opt, arg, 0) != GM_OK) {
            errors++;
            free(arg_c);
            break;
        }
        free(arg_c);
    }

    /* verify options */
    int verify;
    verify = verify_options(mod_gm_opt);

    /* set new options */
    if(errors == 0 && verify == GM_OK) {
    }

    /* read keyfile */
    if(mod_gm_opt->keyfile != NULL && read_keyfile(mod_gm_opt) != GM_OK) {
        errors++;
    }

    if(verify != GM_OK || errors > 0 || mod_gm_opt->debug_level >= GM_LOG_DEBUG) {
        int old_debug = mod_gm_opt->debug_level;
        mod_gm_opt->debug_level = GM_LOG_DEBUG;
        dumpconfig(mod_gm_opt, GM_SEND_GEARMAN_MODE);
        mod_gm_opt->debug_level = old_debug;
    }

    if(errors > 0 || verify != GM_OK) {
        return(GM_ERROR);
    }

    return(GM_OK);
}


/* verify our option */
int verify_options(mod_gm_opt_t *opt) {

    // did we get any server?
    if(opt->server_num == 0) {
        logger( GM_LOG_ERROR, "please specify at least one server\n" );
        return(GM_ERROR);
    }

    // encryption without key?
    if(opt->encryption == GM_ENABLED) {
        if(opt->crypt_key == NULL && opt->keyfile == NULL) {
            logger( GM_LOG_ERROR, "no encryption key provided, please use --key=... or keyfile=...\n");
            return(GM_ERROR);
        }
    }

    if ( mod_gm_opt->result_queue == NULL )
        mod_gm_opt->result_queue = GM_DEFAULT_RESULT_QUEUE;

    return(GM_OK);
}


/* print usage */
void print_usage() {
    printf("usage:\n");
    printf("\n");
    printf("send_gearman [ --debug=<lvl>                ]\n");
    printf("             [ --help|-h                    ]\n");
    printf("\n");
    printf("             [ --config=<configfile>        ]\n");
    printf("\n");
    printf("             [ --server=<server>            ]\n");
    printf("\n");
    printf("             [ --encryption=<yes|no>        ]\n");
    printf("             [ --key=<string>               ]\n");
    printf("             [ --keyfile=<file>             ]\n");
    printf("\n");
    printf("             [ --host=<hostname>            ]\n");
    printf("             [ --service=<servicename>      ]\n");
    printf("             [ --result_queue=<queue>       ]\n");
    printf("             [ --message|-m=<pluginoutput>  ]\n");
    printf("             [ --returncode|-r=<returncode> ]\n");
    printf("\n");
    printf("for sending active checks:\n");
    printf("             [ --active                     ]\n");
    printf("             [ --time=<unixtime>            ]\n");
    printf("             [ --latency=<seconds>          ]\n");
    printf("             [ --exec_time=<seconds>        ]\n");
    printf("\n");
    printf("see README for a detailed explaination of all options.\n");
    printf("\n");

    exit( EXIT_SUCCESS );
}


/* send message to job server */
int send_result() {
    logger( GM_LOG_TRACE, "send_result()\n" );

    struct timeval finish_time;

    if(mod_gm_opt->result_queue == NULL) {
        logger( GM_LOG_ERROR, "got no result queue, please use --result_queue=...\n" );
        return(GM_ERROR);
    }
    if(mod_gm_opt->message == NULL) {
        logger( GM_LOG_ERROR, "got no plugin output, please use --message=...\n" );
        return(GM_ERROR);
    }
    if(mod_gm_opt->host == NULL) {
        logger( GM_LOG_ERROR, "got no hostname, please use --host=...\n" );
        return(GM_ERROR);
    }

    char temp_buffer1[GM_BUFFERSIZE];
    char temp_buffer2[GM_BUFFERSIZE];

    logger( GM_LOG_TRACE, "queue: %s\n", mod_gm_opt->result_queue );
    temp_buffer1[0]='\x0';
    snprintf( temp_buffer1, sizeof( temp_buffer1 )-1, "type=%s\nhost_name=%s\nstart_time=%i.%i\nfinish_time=%i.%i\nlatency=%i.%i\nreturn_code=%i\n",
              mod_gm_opt->active == GM_ENABLED ? "active" : "passive",
              mod_gm_opt->host,
              ( int )mod_gm_opt->time.tv_sec,
              ( int )mod_gm_opt->time.tv_usec,
              ( int )finish_time.tv_sec,
              ( int )finish_time.tv_usec,
              ( int )mod_gm_opt->latency.tv_sec,
              ( int )mod_gm_opt->latency.tv_usec,
              mod_gm_opt->return_code
            );

    if(mod_gm_opt->service != NULL) {
        temp_buffer2[0]='\x0';
        strncat(temp_buffer2, "service_description=", (sizeof(temp_buffer2)-1));
        strncat(temp_buffer2, mod_gm_opt->service, (sizeof(temp_buffer2)-1));
        strncat(temp_buffer2, "\n", (sizeof(temp_buffer2)-1));
        strncat(temp_buffer1, temp_buffer2, (sizeof(temp_buffer1)-1));
    }

    if(mod_gm_opt->message != NULL) {
        temp_buffer2[0]='\x0';
        strncat(temp_buffer2, "output=", (sizeof(temp_buffer2)-1));
        strncat(temp_buffer2, mod_gm_opt->message, (sizeof(temp_buffer2)-1));
        strncat(temp_buffer2, "\n", (sizeof(temp_buffer2)-1));
        strncat(temp_buffer1, temp_buffer2, (sizeof(temp_buffer1)-1));
    }
    strncat(temp_buffer1, "\n", (sizeof(temp_buffer1)-2));

    logger( GM_LOG_TRACE, "data:\n%s\n", temp_buffer1);

    if(add_job_to_queue( &client,
                         mod_gm_opt->server_list,
                         mod_gm_opt->result_queue,
                         NULL,
                         temp_buffer1,
                         GM_JOB_PRIO_NORMAL,
                         GM_DEFAULT_JOB_RETRIES,
                         mod_gm_opt->transportmode
                        ) == GM_OK) {
        logger( GM_LOG_TRACE, "send_result_back() finished successfully\n" );
    }
    else {
        logger( GM_LOG_TRACE, "send_result_back() finished unsuccessfully\n" );
        return(GM_ERROR);
    }
    return(GM_OK);
}
