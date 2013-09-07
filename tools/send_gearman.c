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
#include "gearman_utils.h"

gearman_client_st client;
gearman_client_st client_dup;
int results_sent = 0;

/* work starts here */
int main (int argc, char **argv) {
    int rc;

    /*
     * allocate options structure
     * and parse command line
     */
    if(parse_arguments(argc, argv) != GM_OK) {
        print_usage();
        exit( STATE_UNKNOWN );
    }

    /* set logging */
    mod_gm_opt->debug_level = GM_LOG_INFO;
    mod_gm_opt->logmode     = GM_LOG_MODE_TOOLS;

    /* init crypto functions */
    if(mod_gm_opt->encryption == GM_ENABLED) {
        mod_gm_crypt_init(mod_gm_opt->crypt_key);
    } else {
        mod_gm_opt->transportmode = GM_ENCODE_ONLY;
    }

    /* create client */
    if ( create_client( mod_gm_opt->server_list, &client ) != GM_OK ) {
        printf( "send_gearman UNKNOWN: cannot start client\n" );
        exit( STATE_UNKNOWN );
    }

    /* create duplicate client */
    if ( create_client_dup( mod_gm_opt->dupserver_list, &client_dup ) != GM_OK ) {
        printf( "send_gearman UNKNOWN: cannot start client for duplicate server\n" );
        exit( STATE_UNKNOWN );
    }

    /* send result message */
    signal(SIGALRM, alarm_sighandler);
    rc = send_result();

    gearman_client_free( &client );
    if( mod_gm_opt->dupserver_num )
        gearman_client_free( &client_dup );
    mod_gm_free_opt(mod_gm_opt);

    exit( rc );
}


/* parse command line arguments */
int parse_arguments(int argc, char **argv) {
    int i;
    int verify;
    int errors = 0;
    mod_gm_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);

    /* special default: encryption disabled */
    mod_gm_opt->encryption = GM_DISABLED;

    for(i=1;i<argc;i++) {
        char * arg   = strdup( argv[i] );
        char * arg_c = arg;
        if ( !strcmp( arg, "version" ) || !strcmp( arg, "--version" )  || !strcmp( arg, "-V" ) ) {
            print_version();
        }
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
    verify = verify_options(mod_gm_opt);

    /* read keyfile */
    if(mod_gm_opt->keyfile != NULL && read_keyfile(mod_gm_opt) != GM_OK) {
        errors++;
    }

    if(errors > 0 || verify != GM_OK) {
        return(GM_ERROR);
    }

    return(GM_OK);
}


/* verify our option */
int verify_options(mod_gm_opt_t *opt) {

    /* did we get any server? */
    if(opt->server_num == 0) {
        printf("please specify at least one server\n" );
        return(GM_ERROR);
    }

    /* encryption without key? */
    if(opt->encryption == GM_ENABLED) {
        if(opt->crypt_key == NULL && opt->keyfile == NULL) {
            printf("no encryption key provided, please use --key=... or keyfile=... or disable encryption\n");
            return(GM_ERROR);
        }
    }

    if ( mod_gm_opt->result_queue == NULL )
        mod_gm_opt->result_queue = GM_DEFAULT_RESULT_QUEUE;

    mod_gm_opt->logmode = GM_LOG_MODE_STDOUT;

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
    printf("             [ --timeout|-t=<timeout>       ]\n");
    printf("             [ --delimiter|-d=<delimiter>   ]\n");
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
    printf("             [ --starttime=<unixtime>       ]\n");
    printf("             [ --finishtime=<unixtime>      ]\n");
    printf("             [ --latency=<seconds>          ]\n");
    printf("\n");
    printf("plugin output is read from stdin unless --message is used.\n");
    printf("Use this mode when plugin has multiple lines of plugin output.\n");
    printf("\n");
    printf("Note: When using a delimiter (-d) you may also submit one result\n");
    printf("      for each line.\n");
    printf("      Service Checks:\n");
    printf("      <host_name>[tab]<svc_description>[tab]<return_code>[tab]<plugin_output>[newline]\n");
    printf("\n");
    printf("      Host Checks:\n");
    printf("      <host_name>[tab]<return_code>[tab]<plugin_output>[newline]\n");
    printf("\n");
    printf("see README for a detailed explaination of all options.\n");
    printf("\n");

    mod_gm_free_opt(mod_gm_opt);
    exit( STATE_UNKNOWN );
}


/* send message to job server */
int send_result() {
    char buffer[GM_BUFFERSIZE] = "";
    int size;
    char *ptr1, *ptr2, *ptr3, *ptr4;

    gm_log( GM_LOG_TRACE, "send_result()\n" );

    if(mod_gm_opt->result_queue == NULL) {
        printf( "got no result queue, please use --result_queue=...\n" );
        return( STATE_UNKNOWN );
    }

    /* multiple results */
    if(mod_gm_opt->host == NULL) {
        while(fgets(buffer,sizeof(buffer)-1,stdin)) {
            if(feof(stdin))
                break;

            /* disable alarm */
            alarm(0);

            /* read host_name */
            ptr1=strtok(buffer,mod_gm_opt->delimiter);
            if(ptr1==NULL)
                continue;

            /* get the service description or return code */
            ptr2=strtok(NULL,mod_gm_opt->delimiter);
            if(ptr2==NULL)
                continue;

            /* get the return code or plugin output */
            ptr3=strtok(NULL,mod_gm_opt->delimiter);
            if(ptr3==NULL)
                continue;

            /* get the plugin output - if NULL, this is a host check result */
            ptr4=strtok(NULL,"\n");

            free(mod_gm_opt->host);
            if(mod_gm_opt->service != NULL) {
                free(mod_gm_opt->service);
                mod_gm_opt->service = NULL;
            }
            free(mod_gm_opt->message);

            /* host result */
            if(ptr4 == NULL) {
                mod_gm_opt->host        = strdup(ptr1);
                mod_gm_opt->return_code = atoi(ptr2);
                mod_gm_opt->message     = strdup(ptr3);
            } else {
                /* service result */
                mod_gm_opt->host        = strdup(ptr1);
                mod_gm_opt->service     = strdup(ptr2);
                mod_gm_opt->return_code = atoi(ptr3);
                mod_gm_opt->message     = strdup(ptr4);
            }
            if(submit_result() == STATE_OK) {
                results_sent++;
            } else {
                printf("failed to send result!\n");
                return(STATE_UNKNOWN);
            }
        }
        printf("%d data packet(s) sent to host successfully.\n",results_sent);
        return(STATE_OK);
    }
    /* multi line plugin output */
    else if(mod_gm_opt->message == NULL) {
        /* get all lines from stdin */
        alarm(mod_gm_opt->timeout);
        mod_gm_opt->message = malloc(GM_BUFFERSIZE);
        strcpy(buffer,"");
        size = GM_MAX_OUTPUT;
        while(size > 0 && fgets(buffer,sizeof(buffer)-1,stdin)){
            alarm(0);
            strncat(mod_gm_opt->message, buffer, size);
            size -= strlen(buffer);
        }
        alarm(0);
    }
    return(submit_result());
}

/* submit result */
int submit_result() {
    char * buf;
    char temp_buffer1[GM_BUFFERSIZE];
    char temp_buffer2[GM_BUFFERSIZE];
    struct timeval now;
    struct timeval starttime;
    struct timeval finishtime;

    gettimeofday(&now, NULL);
    if(mod_gm_opt->has_starttime == FALSE) {
        starttime = now;
    } else {
        starttime = mod_gm_opt->starttime;
    }

    if(mod_gm_opt->has_finishtime == FALSE) {
        finishtime = now;
    } else {
        finishtime = mod_gm_opt->finishtime;
    }

    if(mod_gm_opt->has_latency == FALSE) {
        mod_gm_opt->latency.tv_sec  = 0;
        mod_gm_opt->latency.tv_usec = 0;
    }

    /* escape newline */
    buf = gm_escape_newlines(mod_gm_opt->message, GM_DISABLED);
    free(mod_gm_opt->message);
    mod_gm_opt->message = strdup(buf);
    free(buf);

    gm_log( GM_LOG_TRACE, "queue: %s\n", mod_gm_opt->result_queue );
    temp_buffer1[0]='\x0';
    snprintf( temp_buffer1, sizeof( temp_buffer1 )-1, "type=%s\nhost_name=%s\nstart_time=%i.%i\nfinish_time=%i.%i\nlatency=%i.%i\nreturn_code=%i\nsource=send_gearman\n",
              mod_gm_opt->active == GM_ENABLED ? "active" : "passive",
              mod_gm_opt->host,
              (int)starttime.tv_sec,
              (int)starttime.tv_usec,
              (int)finishtime.tv_sec,
              (int)finishtime.tv_usec,
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

    gm_log( GM_LOG_TRACE, "data:\n%s\n", temp_buffer1);

    if(add_job_to_queue( &client,
                         mod_gm_opt->server_list,
                         mod_gm_opt->result_queue,
                         NULL,
                         temp_buffer1,
                         GM_JOB_PRIO_NORMAL,
                         GM_DEFAULT_JOB_RETRIES,
                         mod_gm_opt->transportmode,
                         TRUE
                        ) == GM_OK) {
        gm_log( GM_LOG_TRACE, "send_result_back() finished successfully\n" );

        if( mod_gm_opt->dupserver_num ) {
            if(add_job_to_queue( &client_dup,
                                 mod_gm_opt->dupserver_list,
                                 mod_gm_opt->result_queue,
                                 NULL,
                                 temp_buffer1,
                                 GM_JOB_PRIO_NORMAL,
                                 GM_DEFAULT_JOB_RETRIES,
                                 mod_gm_opt->transportmode,
                                 TRUE
                            ) == GM_OK) {
                gm_log( GM_LOG_TRACE, "send_result_back() finished successfully for duplicate server.\n" );
            }
            else {
                gm_log( GM_LOG_TRACE, "send_result_back() finished unsuccessfully for duplicate server\n" );
            }
        }
    }
    else {
        gm_log( GM_LOG_TRACE, "send_result_back() finished unsuccessfully\n" );
        return( STATE_UNKNOWN );
    }
    return( STATE_OK );
}


/* called when check runs into timeout */
void alarm_sighandler(int sig) {
    gm_log( GM_LOG_TRACE, "alarm_sighandler(%i)\n", sig );

    printf("got no input after %i seconds! Either send plugin output to stdin or use --message=...\n", mod_gm_opt->timeout);

    exit( STATE_UNKNOWN );
}


/* print version */
void print_version() {
    printf("send_gearman: version %s running on libgearman %s\n", GM_VERSION, gearman_version());
    printf("\n");
    exit( STATE_UNKNOWN );
}


/* core log wrapper */
void write_core_log(char *data) {
    printf("core logger is not available for tools: %s", data);
    return;
}
