/******************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein - sven.nierlein@consol.de
 * Copyright (c) 2010 Matthias Flacke - matthias.flacke@gmx.de
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
#include "send_multi.h"
#include "utils.h"
#include "gearman_utils.h"

#include <worker_dummy_functions.c>

gearman_client_st client;
gearman_client_st client_dup;

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
        printf( "send_multi UNKNOWN: cannot start client\n" );
        exit( STATE_UNKNOWN );
    }

    /* create duplicate client */
    if ( create_client_dup( mod_gm_opt->dupserver_list, &client_dup ) != GM_OK ) {
        printf( "send_multi UNKNOWN: cannot start client for duplicate server\n" );
        exit( STATE_UNKNOWN );
    }

    /* send result message */
    signal(SIGALRM, alarm_sighandler);
    rc = read_multi_stream(stdin);
    /* if rc > 0, it contains the number of checks being submitted,
       otherwise its an error code (-1 - WARNING, -2 - CRITICAL, -3 - UNKNOWN) */
    if (rc == 0) {
        printf( "send_multi UNKNOWN: %d check_multi child checks submitted\n", rc );
        rc=STATE_UNKNOWN;
    }
    else if (rc > 0) {
        printf( "send_multi OK: %d check_multi child check%s submitted\n", rc, (rc>1)?"s":"" );
        rc=STATE_OK;
    } else {
        rc*=-1;
    }

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
    mod_gm_opt = gm_malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);

    /* special default: encryption disabled */
    mod_gm_opt->encryption = GM_DISABLED;

    for(i=1;i<argc;i++) {
        char * arg   = gm_strdup( argv[i] );
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

    /* no server specified? then default to localhost */
    if(opt->server_num == 0) {
        add_server(&opt->server_num, opt->server_list, "localhost");
    }

    /* host is mandatory */
    if(opt->host == NULL) {
        printf("got no hostname, please use --host=...\n" );
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

    return(GM_OK);
}


/* print usage */
void print_usage() {
    printf("usage:\n");
    printf("\n");
    printf("send_multi   [ --debug=<lvl>            ]\n");
    printf("             [ --help|-h                ]\n");
    printf("\n");
    printf("             [ --config=<configfile>    ]\n");
    printf("\n");
    printf("             [ --server=<server>        ]\n");
    printf("                 default:localhost       \n");
    printf("\n");
    printf("             [ --encryption=<yes|no>    ]\n");
    printf("                 default:no              \n");
    printf("             [ --key=<string>           ]\n");
    printf("             [ --keyfile=<file>         ]\n");
    printf("\n");
    printf("             [ --host=<hostname>        ]\n");
    printf("             [ --result_queue=<queue>   ]\n");
    printf("                 default:check_results   \n");
    printf("             [ --timeout=<seconds>      ]\n");
    printf("\n");
    printf("For details see\n");
    printf("http://my-plugin.de/wiki/projects/check_multi/feed_passive\n");
    printf("\n");

    exit( STATE_UNKNOWN );
}


/* send message to job server */
int send_result() {
    char * buf;
    char temp_buffer1[GM_BUFFERSIZE];
    char temp_buffer2[GM_BUFFERSIZE];

    gm_log( GM_LOG_TRACE, "send_result()\n" );

    if(mod_gm_opt->result_queue == NULL) {
        printf( "got no result queue, please use --result_queue=...\n" );
        return(GM_ERROR);
    }

    /* escape newline */
    buf = gm_escape_newlines(mod_gm_opt->message, GM_DISABLED);
    free(mod_gm_opt->message);
    mod_gm_opt->message = gm_malloc(GM_BUFFERSIZE);
    snprintf(mod_gm_opt->message, GM_BUFFERSIZE, "%s", buf);
    free(buf);

    gm_log( GM_LOG_TRACE, "queue: %s\n", mod_gm_opt->result_queue );
    temp_buffer1[0]='\x0';
    snprintf( temp_buffer1, sizeof( temp_buffer1 )-1, "type=%s\nhost_name=%s\nstart_time=%i.%i\nfinish_time=%i.%i\nreturn_code=%i\nsource=send_multi\n",
              mod_gm_opt->active == GM_ENABLED ? "active" : "passive",
              mod_gm_opt->host,
              (int)mod_gm_opt->starttime.tv_sec,
              (int)mod_gm_opt->starttime.tv_usec,
              (int)mod_gm_opt->finishtime.tv_sec,
              (int)mod_gm_opt->finishtime.tv_usec,
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
            if(add_job_to_queue( &client,
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
        return(GM_ERROR);
    }
    return(GM_OK);
}

/* called when check runs into timeout */
void alarm_sighandler(int sig) {
    gm_log( GM_LOG_TRACE, "alarm_sighandler(%i)\n", sig );

    printf("Timeout after %d seconds - got no input! Send plugin output to stdin.\n", mod_gm_opt->timeout);

    exit( STATE_CRITICAL );
}


/* print version */
void print_version() {
    printf("send_gearman: version %s running on libgearman %s\n", GM_VERSION, gearman_version());
    printf("\n");
    exit( STATE_UNKNOWN );
}

int read_multi_stream(FILE *stream) {
    char buffer[GM_BUFFERSIZE+1];
    unsigned long buflen=0L;
    unsigned long bytes_read=0L;
    char *bufstart=NULL;
    char *bufend=NULL;
    int count=0;
    struct timeval end_time;

    gettimeofday(&end_time, NULL);

    do {
        /* opening tag <CHILD> found? read from buffer start with maximum buffer len */
        if ((bufstart=(char *)memmem(buffer,buflen,"<CHILD>",strlen("<CHILD>"))) != NULL) {

            /* closing tag </CHILD> found? read after <CHILD> with rest of buffer len */
            if ((bufend=(char *)memmem(bufstart,buflen-(bufstart-buffer),"</CHILD>",strlen("</CHILD>"))) != NULL) {

                gm_log( GM_LOG_TRACE, "\tXML chunk %d found: buffer position %3d-%3d length %d bytes\n", count, bufstart-buffer, bufend-buffer, bufend-bufstart);
                /* count valid chunks */
                count++;

                /* identify CHILD chunk */
                bufstart+=strlen("<CHILD>");

                /* if valid check_multi chunk found, send the result*/
                if (read_child_check(bufstart,bufend,&end_time)) {
                    if (send_result() == GM_ERROR) {
                        count--;
                    }
                } else {
                    count--;
                }

                /* shift input rest to buffer start, create space for subsequent reads */
                memmove(buffer,bufend+strlen("</CHILD>"),buflen-(bufend+strlen("</CHILD>")-buffer));
                buflen-=bufend+strlen("</CHILD>")-buffer;

            /* start <CHILD> tag found, but no closing tag </CHILD>, buffer too small? */
            } else {
                buflen=0L;
                printf("send_multi UNKNOWN: no closing tag </CHILD> within buffer, buffer size too small? discarding buffer, %ld bytes now\n", buflen);
                return -STATE_UNKNOWN;
            }
            gm_log( GM_LOG_TRACE, "\tbuflen after XML chunk parsing:%ld\n", buflen);

        /* neither <CHILD> nor </CHILD> found, discard buffer */
        } else {

            /* no chunks found? then check for message in STDIN */
            if (!count) {
                unsigned long i;
                /* check buffer for ASCII characters */
                for (i=0; i<buflen && buffer[i] && isascii(buffer[i]); i++)
                    ;
                /* ASCIIZ string? then print messages */
                if (buffer[i] == '\0' && i) {
                    printf("send_multi UNKNOWN: error msg in input buffer: %s\n", buffer);
                    return -STATE_UNKNOWN;
                }
            }

            /* discard whole buffer but continue */
            buflen=0L;
            gm_log( GM_LOG_TRACE, "Error: no starting tag <CHILD> within buffer - discarding buffer, buflen now %ld bytes\n", buflen);
        }

        gm_log( GM_LOG_TRACE, "\ttrying to fill up buffer with %ld bytes from offset %ld\n", GM_BUFFERSIZE-buflen, buflen);

        /* read one block of data, or less bytes, if there is still data left */
        alarm(mod_gm_opt->timeout);
        if ((bytes_read=fread(buffer+buflen, 1, GM_BUFFERSIZE-buflen, stream)) == 0L) {

            /* break if zero read was caused by an error */
            if (!feof(stream)) {
                perror("fread");
                return -STATE_CRITICAL;
            }
        } else {

            /* adjust block len */
            buflen+=bytes_read;
        }
        gm_log( GM_LOG_TRACE, "\tread %ld bytes, %ld bytes remaining in buffer\n", bytes_read, buflen);
    } while (buflen > 0);
    buffer[buflen] = '\0';
    return count;
}

int read_child_check(char *bufstart, char *bufend, struct timeval * end_time) {
    char *attribute  = NULL;
    char *attribute2 = NULL;
    char *attribute3 = NULL;
    char *error      = NULL;
    char temp_buffer[GM_BUFFERSIZE];
    double end_time_d;
    struct timeval start_time;

    /* child check number */
    if ((attribute=read_multi_attribute(bufstart,bufend,"no")) == NULL) {
        return 0;
    } else {
        /* skip parent check */
        if (!strcmp(attribute,"0")) {
            return 0;
        }
        gm_log( GM_LOG_TRACE, "child check: %d\n", atoi(attribute));
    }

    /* service description */
    if ((attribute=read_multi_attribute(bufstart,bufend,"name")) == NULL)
        return 0;
    mod_gm_opt->service=gm_strdup(attribute);
    gm_log( GM_LOG_TRACE, "service_description: %s\n", mod_gm_opt->service);

    /* return code */
    if ((attribute=read_multi_attribute(bufstart,bufend,"rc")) == NULL)
        return 0;
    mod_gm_opt->return_code=atoi(attribute);
    gm_log( GM_LOG_TRACE, "mod_gm_opt->return_code: %d\n", mod_gm_opt->return_code);

    /* runtime */
    if ((attribute=read_multi_attribute(bufstart,bufend,"runtime")) == NULL)
        return 0;
    end_time_d   = timeval2double(end_time);
    double2timeval(end_time_d - atof(attribute), &start_time);

    mod_gm_opt->starttime.tv_sec  = start_time.tv_sec;
    mod_gm_opt->starttime.tv_usec = start_time.tv_usec;
    gm_log( GM_LOG_TRACE, "starttime: %d.%d\n", mod_gm_opt->starttime.tv_sec, mod_gm_opt->starttime.tv_usec);


    /* end time is the execution time of send_multi itself */
    mod_gm_opt->finishtime.tv_sec  = end_time->tv_sec;
    mod_gm_opt->finishtime.tv_usec = end_time->tv_usec;
    gm_log( GM_LOG_TRACE, "endtime: %d.%d\n", mod_gm_opt->finishtime.tv_sec, mod_gm_opt->finishtime.tv_usec);

    /* message */
    if ((attribute=read_multi_attribute(bufstart,bufend,"output")) == NULL)
        return 0;

    /* stderr */
    if ((error=read_multi_attribute(bufstart,bufend,"error")) == NULL) {
        return 0;
    /* if error found: 'error' -> ' [error]' */
    } else if (*error) {
        *(--error)='[';
        *(--error)=' ';
        strcat(error,"]");
    }

    /* performance data with multi headers */
    if ((attribute2=read_multi_attribute(bufstart,bufend,"performance")) == NULL) {
        snprintf( temp_buffer, sizeof( temp_buffer )-1, "%s%s", decode_xml(attribute), decode_xml(error));
    } else if ((attribute3=read_multi_attribute(bufstart,bufend,"pplugin")) == NULL) {
        return 0;
    } else {
        attribute2 = trim(attribute2);
        attribute2 = decode_xml(attribute2);

        /* do we have a single quote performance label?
           then single quote the whole multi header */
        if (*attribute2 == '\'') {
            attribute2++;
            snprintf( temp_buffer, sizeof( temp_buffer )-1, "%s%s|\'%s::%s::%s", decode_xml(attribute), decode_xml(error), mod_gm_opt->service, decode_xml(attribute3), decode_xml(attribute2));
        /* normal header without single quotes */
        } else {
            snprintf( temp_buffer, sizeof( temp_buffer )-1, "%s%s|%s::%s::%s", decode_xml(attribute), decode_xml(error), mod_gm_opt->service, decode_xml(attribute3), decode_xml(attribute2));
        }
    }
    mod_gm_opt->message=gm_strdup(temp_buffer);
    gm_log( GM_LOG_TRACE, "mod_gm_opt->message: %s\n", mod_gm_opt->message);

    return 1;
}

char *read_multi_attribute(char *bufstart, char *bufend, char *element) {
    char start_element[GM_BUFFERSIZE], end_element[GM_BUFFERSIZE];
    sprintf(start_element, "<%s>", element);
    sprintf(end_element, "</%s>", element);

    if ((bufstart=(char *)memmem(bufstart,bufend-bufstart,start_element,strlen(start_element))) == NULL) {
        gm_log( GM_LOG_TRACE, "\tread_multi_attribute: start element \'%s\' not found\n", start_element);
        return NULL;
    }
    bufstart+=strlen(start_element);
    if ((bufend=(char *)memmem(bufstart,bufend-bufstart,end_element,strlen(end_element))) == NULL) {
        gm_log( GM_LOG_TRACE, "\tread_multi_attribute: end element \'%s\' not found\n", end_element);
        return NULL;
    }
    *bufend='\0';
    return bufstart;
}

char *decode_xml(char *string) {
    struct decode{
            char c;
            char *enc_string;
    } dtab[] = {
            { '>',  "&gt;"   },
            { '<',  "&lt;"   },
            { '&',  "&amp;"  },
            { '\'', "&#039;" },
            { '|',  "&#124;" },
    };
    int i;
    char *found;

    /* foreach XML decode pair */
    for (i=0; i<(int)(sizeof(dtab)/sizeof(struct decode)); i++) {
    /* while XML encoding strings found */
        while ((found=strstr(string, dtab[i].enc_string)) != NULL) {
            /* replace string with character */
            *found=dtab[i].c;
            /* shift rest of string after character */
            strcpy(found+1, found+strlen(dtab[i].enc_string));
        }
    }
    return string;
}


/* core log wrapper */
void write_core_log(char *data) {
    printf("core logger is not available for tools: %s", data);
    return;
}

