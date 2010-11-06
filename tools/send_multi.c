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
#include "worker_logger.h"
#include "gearman.h"

gearman_client_st client;

/* work starts here */
int main (int argc, char **argv) {
    int rc;

    /*
     * allocate options structure
     * and parse command line
     */
    if(parse_arguments(argc, argv) != GM_OK) {
        print_usage();
        exit( EXIT_FAILURE );
    }

    /* init crypto functions */
    if(mod_gm_opt->encryption == GM_ENABLED) {
        mod_gm_crypt_init(mod_gm_opt->crypt_key);
    } else {
        mod_gm_opt->transportmode = GM_ENCODE_ONLY;
    }

    /* create client */
    if ( create_client( mod_gm_opt->server_list, &client ) != GM_OK ) {
        printf( "cannot start client\n" );
        exit( EXIT_FAILURE );
    }

    /* send result message */
    signal(SIGALRM, alarm_sighandler);
    rc = read_multi_stream(stdin);
    /* if rc > 0, it contains the number of checks being submitted, 
       otherwise its an error code (-1 - WARNING, -2 - CRITICAL, -3 - UNKNOWN) */
    if (rc >= 0) {
	    logger( GM_LOG_INFO, "%d check_multi child check%s submitted\n", rc, (rc>1)?"s":"" );
    } else {
	    rc*=-1;
    }

    gearman_client_free( &client );
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
        lc(arg);
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
	opt->server_list[opt->server_num] = strdup("localhost");
	opt->server_num++;
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
    printf("                 default:result_queue    \n");
    printf("             [ --timeout=<seconds>      ]\n");
    printf("\n");
    printf("For details see\n");
    printf("http://my-plugin.de/wiki/projects/check_multi/feed_passive\n");
    printf("\n");

    exit( EXIT_SUCCESS );
}


/* send message to job server */
int send_result() {
    char * buf;
    char temp_buffer1[GM_BUFFERSIZE];
    char temp_buffer2[GM_BUFFERSIZE];

    logger( GM_LOG_TRACE, "send_result()\n" );

    if(mod_gm_opt->result_queue == NULL) {
        printf( "got no result queue, please use --result_queue=...\n" );
        return(GM_ERROR);
    }

    /* escape newline */
    buf = escape_newlines(mod_gm_opt->message);
    free(mod_gm_opt->message);
    mod_gm_opt->message = malloc(GM_BUFFERSIZE);
    snprintf(mod_gm_opt->message, GM_BUFFERSIZE, "%s", buf);
    free(buf);

    logger( GM_LOG_TRACE, "queue: %s\n", mod_gm_opt->result_queue );
    temp_buffer1[0]='\x0';
    snprintf( temp_buffer1, sizeof( temp_buffer1 )-1, "type=%s\nhost_name=%s\nstart_time=%i.%i\nfinish_time=%i.%i\nlatency=%i.%i\nreturn_code=%i\n",
              mod_gm_opt->active == GM_ENABLED ? "active" : "passive",
              mod_gm_opt->host,
              (int)mod_gm_opt->starttime.tv_sec,
              (int)mod_gm_opt->starttime.tv_usec,
              (int)mod_gm_opt->finishtime.tv_sec,
              (int)mod_gm_opt->finishtime.tv_usec,
              (int)mod_gm_opt->latency.tv_sec,
              (int)mod_gm_opt->latency.tv_usec,
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

/* called when check runs into timeout */
void alarm_sighandler(int sig) {
    logger( GM_LOG_TRACE, "alarm_sighandler(%i)\n", sig );

    printf("got no input! Either send plugin output to stdin or use --message=...\n");

    exit(EXIT_FAILURE);
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

	//char outbuf[GM_BUFFERSIZE+1];

	do {
		/* opening tag <CHILD> found? read from buffer start with maximum buffer len */
		if ((bufstart=(char *)memmem(buffer,buflen,"<CHILD>",strlen("<CHILD>"))) != NULL) { 

			/* closing tag </CHILD> found? read after <CHILD> with rest of buffer len */
		    	if ((bufend=(char *)memmem(bufstart,buflen-(bufstart-buffer),"</CHILD>",strlen("</CHILD>"))) != NULL) {

				logger( GM_LOG_TRACE, "\tXML chunk %d found: buffer position %3d-%3d length %d bytes\n", count, bufstart-buffer, bufend-buffer, bufend-bufstart);
				/* count valid chunks */
				count++;

				/* identify CHILD chunk */
				bufstart+=strlen("<CHILD>");

				/* if valid check_multi chunk found, send the result*/
				if (read_child_check(bufstart,bufend)) {
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
				logger( GM_LOG_ERROR, "Error: no closing tag </CHILD> within buffer, buffer size too small? discarding buffer, %ld bytes now\n", buflen);
				return -1;
			}
			logger( GM_LOG_TRACE, "\tbuflen after XML chunk parsing:%ld\n", buflen);

		/* neither <CHILD> nor </CHILD> found, discard buffer */
		} else {
			/* discard whole buffer but continue */
			buflen=0L;
			logger( GM_LOG_TRACE, "Error: no starting tag <CHILD> within buffer - discarding buffer, buflen now %ld bytes\n", buflen);
		}
		
		logger( GM_LOG_TRACE, "\ttrying to fill up buffer with %ld bytes from offset %ld\n", GM_BUFFERSIZE-buflen, buflen);

		/* read one block of data, or less bytes, if there is still data left */
		alarm(mod_gm_opt->timeout);
		if ((bytes_read=fread(buffer+buflen, 1, GM_BUFFERSIZE-buflen, stream)) == 0L) {

			/* break if zero read was caused by an error */
			if (!feof(stream)) {
				perror("fread");
				return -2;
			}
		} else {

			/* adjust block len */
			buflen+=bytes_read;
		}
		logger( GM_LOG_TRACE, "\tread %ld bytes, %ld bytes remaining in buffer\n", bytes_read, buflen);
	} while (buflen > 0);
	return count;
}

int read_child_check(char *bufstart, char *bufend) {
	char *attribute=NULL;

	/* child check number */
	if ((attribute=read_multi_attribute(bufstart,bufend,"no")) == NULL) {
		return 0;
	} else {
		/* skip parent check */
		if (!strcmp(attribute,"0")) {
			return 0;
		}
		logger( GM_LOG_TRACE, "child check: %d\n", atoi(attribute));
	}

	/* service description */
	if ((attribute=read_multi_attribute(bufstart,bufend,"name")) == NULL) {
		return 0;
	} else {
		mod_gm_opt->service=strdup(attribute);
		logger( GM_LOG_TRACE, "service_description: %s\n", mod_gm_opt->service);
	}

	/* return code */
	if ((attribute=read_multi_attribute(bufstart,bufend,"rc")) == NULL) {
		return 0;
	} else {
		mod_gm_opt->return_code=atoi(attribute);
		logger( GM_LOG_TRACE, "mod_gm_opt->return_code: %d\n", mod_gm_opt->return_code);
	}

	/* start time */
	if ((attribute=read_multi_attribute(bufstart,bufend,"starttime")) == NULL) {
		return 0;
	} else {
		mod_gm_opt->starttime.tv_sec=atoi(strtok(attribute, "."));
		mod_gm_opt->starttime.tv_usec=atoi(strtok(NULL, "."));
		logger( GM_LOG_TRACE, "starttime: %d.%d\n", mod_gm_opt->starttime.tv_sec, mod_gm_opt->starttime.tv_usec);
	}

	/* end time */
	if ((attribute=read_multi_attribute(bufstart,bufend,"endtime")) == NULL) {
		return 0;
	} else {
		mod_gm_opt->finishtime.tv_sec=atoi(strtok(attribute, "."));
		mod_gm_opt->finishtime.tv_usec=atoi(strtok(NULL, "."));
		logger( GM_LOG_TRACE, "endtime: %d.%d\n", mod_gm_opt->finishtime.tv_sec, mod_gm_opt->finishtime.tv_usec);
	}

	/* message */
	if ((attribute=read_multi_attribute(bufstart,bufend,"output")) == NULL) {
		return 0;
	} else {
		mod_gm_opt->message=strdup(decode_xml(attribute));
		logger( GM_LOG_TRACE, "mod_gm_opt->message: %s\n", mod_gm_opt->message);
	}
	return 1;
}

char *read_multi_attribute(char *bufstart, char *bufend, char *element) {
	char start_element[GM_BUFFERSIZE], end_element[GM_BUFFERSIZE];
	sprintf(start_element, "<%s>", element);
	sprintf(end_element, "</%s>", element);

	if ((bufstart=(char *)memmem(bufstart,bufend-bufstart,start_element,strlen(start_element))) == NULL) {
		logger( GM_LOG_TRACE, "\tread_multi_attribute: start element \'%s\' not found\n", start_element);
		return NULL;
	}
	bufstart+=strlen(start_element);
	if ((bufend=(char *)memmem(bufstart,bufend-bufstart,end_element,strlen(end_element))) == NULL) {
		logger( GM_LOG_TRACE, "\tread_multi_attribute: end element \'%s\' not found\n", end_element);
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
                { '>', "&gt;"  },
                { '<', "&lt;"  },
                { '&', "&amp;" },
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
