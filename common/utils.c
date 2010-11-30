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

#include "utils.h"
#include "gm_log.h"
#include "crypt.h"
#include "base64.h"


/* escapes newlines in a string */
char *escape_newlines(char *rawbuf) {
    char *newbuf=NULL;
    register int x,y;

    if(rawbuf==NULL)
        return NULL;

    /* allocate enough memory to escape all chars if necessary */
    if((newbuf=malloc((strlen(rawbuf)*2)+1))==NULL)
        return NULL;

    for(x=0,y=0;rawbuf[x]!=(char)'\x0';x++){

        /* escape backslashes */
        if(rawbuf[x]=='\\'){
            newbuf[y++]='\\';
            newbuf[y++]='\\';
        }

        /* escape newlines */
        else if(rawbuf[x]=='\n'){
            newbuf[y++]='\\';
            newbuf[y++]='n';
        }

        else
            newbuf[y++]=rawbuf[x];
    }
    newbuf[y]='\x0';

    return newbuf;
}


/* convert exit code to int */
int real_exit_code(int code) {
    if( code == -1 ){
        return(3);
    } else {
        if(WIFSIGNALED( code)) {
            return(128 + WTERMSIG( code ));
        }
        if(WIFEXITED(code)) {
            return(WEXITSTATUS(code));
        }
    }
    return(0);
}


/* initialize encryption */
void mod_gm_crypt_init(char * key) {
    if(strlen(key) < 8) {
        gm_log( GM_LOG_INFO, "encryption key should be at least 8 bytes!\n" );
    }
    mod_gm_aes_init(key);
    return;
}


/* encrypt text with given key */
int mod_gm_encrypt(char ** encrypted, char * text, int mode) {
    int size;
    unsigned char * crypted;
    char * base64;

    if(mode == GM_ENCODE_AND_ENCRYPT) {
        size = mod_gm_aes_encrypt(&crypted, text);
    }
    else {
        crypted = (unsigned char*)strdup(text);
        size    = strlen(text);
    }

    /* now encode in base64 */
    base64 = malloc(GM_BUFFERSIZE);
    base64[0] = 0;
    base64_encode(crypted, size, base64, GM_BUFFERSIZE);
    free(*encrypted);
    free(crypted);
    *encrypted = base64;
    return strlen(base64);
}


/* decrypt text with given key */
void mod_gm_decrypt(char ** decrypted, char * text, int mode) {
    unsigned char * buffer = malloc(sizeof(unsigned char) * GM_BUFFERSIZE);

    /* first decode from base64 */
    size_t bsize = base64_decode(text, buffer, GM_BUFFERSIZE);
    if(mode == GM_ENCODE_AND_ENCRYPT) {
        /* then decrypt */
        mod_gm_aes_decrypt(decrypted, buffer, bsize);
    }
    else  {
        char debase64[GM_BUFFERSIZE];
        strncpy(debase64, (char*)buffer, bsize);
        debase64[bsize] = '\0';
        strcpy(*decrypted, debase64);
    }
    free(buffer);
    return;
}


/* test for file existence */
int file_exists (char * fileName) {
    struct stat buf;
    int i = stat ( fileName, &buf );
    /* File found */
    if( i == 0 ) {
        return 1;
    }
    return 0;
}


/* trim left spaces */
char *ltrim(char *s) {
    if(s == NULL)
        return NULL;
    while(isspace(*s))
        s++;
    return s;
}


/* trim right spaces */
char *rtrim(char *s) {
    char *back;
    if(s == NULL)
        return NULL;
    back = s + strlen(s);
    while(isspace(*--back));
    *(back+1) = '\0';
    return s;
}


/* trim spaces */
char *trim(char *s) {
    if(s == NULL)
        return NULL;
    return rtrim(ltrim(s));
}


/* make string lowercase */
char * lc(char * str) {
    int i;
    if(str == NULL)
        return NULL;
    for( i = 0; str[ i ]; i++)
        str[ i ] = tolower( str[ i ] );
    return str;
}


/* set empty default options */
int set_default_options(mod_gm_opt_t *opt) {
    int i;

    opt->set_queues_by_hand = 0;
    opt->result_workers     = 1;
    opt->crypt_key          = NULL;
    opt->result_queue       = NULL;
    opt->keyfile            = NULL;
    opt->logfile            = NULL;
    opt->logfile_fp         = NULL;
    opt->message            = NULL;
    opt->return_code        = 0;
    opt->timeout            = 10;
    opt->debug_level        = GM_LOG_INFO;
    opt->perfdata           = GM_DISABLED;
    opt->hosts              = GM_DISABLED;
    opt->services           = GM_DISABLED;
    opt->events             = GM_DISABLED;
    opt->job_timeout        = GM_DEFAULT_JOB_TIMEOUT;
    opt->encryption         = GM_ENABLED;
    opt->pidfile            = NULL;
    opt->debug_result       = GM_DISABLED;
    opt->max_age            = GM_DEFAULT_JOB_MAX_AGE;
    opt->min_worker         = GM_DEFAULT_MIN_WORKER;
    opt->max_worker         = GM_DEFAULT_MAX_WORKER;
    opt->transportmode      = GM_ENCODE_AND_ENCRYPT;
    opt->daemon_mode        = GM_DISABLED;
    opt->fork_on_exec       = GM_ENABLED;
    opt->idle_timeout       = GM_DEFAULT_IDLE_TIMEOUT;
    opt->max_jobs           = GM_DEFAULT_MAX_JOBS;
    opt->identifier         = NULL;

    opt->host               = NULL;
    opt->service            = NULL;

    opt->server_num         = 0;
    for(i=0;i<=GM_LISTSIZE;i++)
        opt->server_list[i] = NULL;
    opt->hostgroups_num     = 0;
    for(i=0;i<=GM_LISTSIZE;i++)
        opt->hostgroups_list[i] = NULL;
    opt->servicegroups_num  = 0;
    for(i=0;i<=GM_LISTSIZE;i++)
        opt->servicegroups_list[i] = NULL;
    opt->local_hostgroups_num     = 0;
    for(i=0;i<=GM_LISTSIZE;i++)
        opt->local_hostgroups_list[i] = NULL;
    opt->local_servicegroups_num  = 0;
    for(i=0;i<=GM_LISTSIZE;i++)
        opt->local_servicegroups_list[i] = NULL;

    return(GM_OK);
}


/* parse an option value to yes/no */
int parse_yes_or_no(char*value, int dfl) {
    if(value == NULL)
        return dfl;

    lc(value);
    if(!strcmp( value, "yes" ))
        return(GM_ENABLED);
    if(!strcmp( value, "on" ))
        return(GM_ENABLED);
    if(!strcmp( value, "true" ))
        return(GM_ENABLED);
    if(!strcmp( value, "1" ))
        return(GM_ENABLED);

    if(!strcmp( value, "no" ))
        return(GM_DISABLED);
    if(!strcmp( value, "off" ))
        return(GM_DISABLED);
    if(!strcmp( value, "false" ))
        return(GM_DISABLED);
    if(!strcmp( value, "0" ))
        return(GM_DISABLED);

    return dfl;
}


/* parse one line of args into the given struct */
int parse_args_line(mod_gm_opt_t *opt, char * arg, int recursion_level) {
    char *key;
    char *value;

    gm_log( GM_LOG_TRACE, "parse_args_line(%s, %d)\n", arg, recursion_level);

    key   = strsep( &arg, "=" );
    value = strsep( &arg, "\x0" );

    if ( key == NULL )
        return(GM_OK);

    lc(key);
    key   = trim(key);
    value = trim(value);

    /* skip leading hyphen */
    while(key[0] == '-')
        key++;

    /* daemon mode */
    if ( !strcmp( key, "daemon" ) ||  !strcmp( key, "d" ) ) {
        opt->daemon_mode = parse_yes_or_no(value, GM_ENABLED);
    }

    /* perfdata */
    else if (   !strcmp( key, "perfdata" ) ) {
        opt->set_queues_by_hand++;
        opt->perfdata = parse_yes_or_no(value, GM_ENABLED);
    }

    /* hosts */
    else if (   !strcmp( key, "hosts" )
        || !strcmp( key, "host" )
        ) {
        opt->set_queues_by_hand++;
        opt->hosts = parse_yes_or_no(value, GM_ENABLED);
    }

    /* services */
    else if (   !strcmp( key, "services" )
             //|| !strcmp( key, "service" )
            ) {
        opt->set_queues_by_hand++;
        opt->services = parse_yes_or_no(value, GM_ENABLED);
    }

    /* eventhandler */
    else if (   !strcmp( key, "events" )
             || !strcmp( key, "event" )
             || !strcmp( key, "eventhandlers" )
             || !strcmp( key, "eventhandler" )
            ) {
        opt->set_queues_by_hand++;
        opt->events = parse_yes_or_no(value, GM_ENABLED);
    }

    /* debug-result */
    else if ( !strcmp( key, "debug-result" ) ) {
        opt->debug_result = parse_yes_or_no(value, GM_ENABLED);
    }

    /* encryption */
    else if ( !strcmp( key, "encryption" ) ) {
        opt->encryption = parse_yes_or_no(value, GM_ENABLED);
    }

    /* fork_on_exec */
    else if ( !strcmp( key, "fork_on_exec" ) ) {
        opt->fork_on_exec = parse_yes_or_no(value, GM_ENABLED);
    }

    /* active */
    else if (   !strcmp( key, "active" ) ) {
        opt->active = parse_yes_or_no(value, GM_ENABLED);
    }

    if ( value == NULL )
        return(GM_OK);

    /* debug */
    if ( !strcmp( key, "debug" ) ) {
        opt->debug_level = atoi( value );
        if(opt->debug_level < 0) { opt->debug_level = 0; }
    }

    /* result worker */
    else if ( !strcmp( key, "result_workers" ) ) {
        opt->result_workers = atoi( value );
        if(opt->result_workers > GM_LISTSIZE) { opt->result_workers = GM_LISTSIZE; }
        if(opt->result_workers < 0) { opt->result_workers = 0; }
    }

    /* return code */
    else if (   !strcmp( key, "returncode" )
             || !strcmp( key, "r" )
       ) {
        opt->return_code = atoi( value );
        if(opt->return_code > 3) { return(GM_ERROR); }
        if(opt->return_code < 0) { return(GM_ERROR); }
    }

    /* result_queue */
    else if (   !strcmp( key, "result_queue" ) ) {
        opt->result_queue = strdup( value );
    }

    /* message */
    else if (   !strcmp( key, "message" )
             || !strcmp( key, "m" )
            ) {
        opt->message = strdup( value );
    }

    /* host */
    else if (   !strcmp( key, "host" ) ) {
        opt->host = strdup( value );
    }

    /* service */
    else if (   !strcmp( key, "service" ) ) {
        opt->service = strdup( value );
    }

    /* latency */
    else if ( !strcmp( key, "latency" ) ) {
        string2timeval(value, &opt->latency);
    }

    /* start time */
    else if ( !strcmp( key, "starttime" ) ) {
        string2timeval(value, &opt->starttime);
    }

    /* finish time */
    else if ( !strcmp( key, "finishtime" ) ) {
        string2timeval(value, &opt->finishtime);
    }

    /* configfile */
    else if (   !strcmp( key, "config" )
             || !strcmp( key, "configfile" )
            ) {
        if(read_config_file(opt, value, ++recursion_level) != GM_OK) {
            recursion_level--;
            return GM_ERROR;
        }
    }

    /* key / password */
    else if (   !strcmp( key, "key" )
             || !strcmp( key, "password" )
            ) {
        opt->crypt_key = strdup( value );
    }

    /* keyfile / passwordfile */
    else if (   !strcmp( key, "keyfile" )
             || !strcmp( key, "passwordfile" )
            ) {
        opt->keyfile = strdup( value );
    }

    /* pidfile */
    else if ( !strcmp( key, "pidfile" ) ) {
        opt->pidfile = strdup( value );
    }

    /* logfile */
    else if ( !strcmp( key, "logfile" ) ) {
        opt->logfile = strdup( value );
    }

    /* identifier */
    else if ( !strcmp( key, "identifier" ) ) {
        opt->identifier = strdup( value );
    }

    /* timeout */
    else if ( !strcmp( key, "timeout" ) ) {
        opt->timeout = atoi( value );
        if(opt->timeout < 0) { opt->timeout = 10; }
    }

    /* job_timeout */
    else if ( !strcmp( key, "job_timeout" ) ) {
        opt->job_timeout = atoi( value );
        if(opt->job_timeout < 1) { opt->job_timeout = 1; }
    }

    /* min-worker */
    else if ( !strcmp( key, "min-worker" ) ) {
        opt->min_worker = atoi( value );
        if(opt->min_worker <= 0) { opt->min_worker = 1; }
    }

    /* max-worker */
    else if ( !strcmp( key, "max-worker" )  ) {
        opt->max_worker = atoi( value );
        if(opt->max_worker <= 0) { opt->max_worker = 1; }
    }

    /* max-age */
    else if ( !strcmp( key, "max-age" ) ) {
        opt->max_age = atoi( value );
        if(opt->max_age <= 0) { opt->max_age = 1; }
    }

    /* idle-timeout */
    else if ( !strcmp( key, "idle-timeout" ) ) {
        opt->idle_timeout = atoi( value );
        if(opt->idle_timeout < 0) { opt->job_timeout = GM_DEFAULT_IDLE_TIMEOUT; }
    }

    /* max-jobs */
    else if ( !strcmp( key, "max-jobs" ) ) {
        opt->max_jobs = atoi( value );
        if(opt->max_jobs < 0) { opt->max_jobs = GM_DEFAULT_MAX_JOBS; }
    }

    /* server */
    else if ( !strcmp( key, "server" ) ) {
        char *servername;
        while ( (servername = strsep( &value, "," )) != NULL ) {
            servername = trim(servername);
            if ( strcmp( servername, "" ) ) {
                opt->server_list[opt->server_num] = strdup(servername);
                opt->server_num++;
            }
        }
    }

    /* servicegroups */
    else if (   !strcmp( key, "servicegroups" )
             || !strcmp( key, "servicegroup" )
            ) {
        char *groupname;
        while ( (groupname = strsep( &value, "," )) != NULL ) {
            groupname = trim(groupname);
            if ( strcmp( groupname, "" ) ) {
                opt->servicegroups_list[opt->servicegroups_num] = strdup(groupname);
                opt->servicegroups_num++;
                opt->set_queues_by_hand++;
            }
        }
    }

    /* hostgroups */
    else if (   !strcmp( key, "hostgroups" )
             || !strcmp( key, "hostgroup" ) ) {
        char *groupname;
        while ( (groupname = strsep( &value, "," )) != NULL ) {
            groupname = trim(groupname);
            if ( strcmp( groupname, "" ) ) {
                opt->hostgroups_list[opt->hostgroups_num] = strdup(groupname);
                opt->hostgroups_num++;
                opt->set_queues_by_hand++;
            }
        }
    }

    /* local servicegroups */
    else if (   !strcmp( key, "localservicegroups" )
             || !strcmp( key, "localservicegroup" )
            ) {
        char *groupname;
        while ( (groupname = strsep( &value, "," )) != NULL ) {
            groupname = trim(groupname);
            if ( strcmp( groupname, "" ) ) {
                opt->local_servicegroups_list[opt->local_servicegroups_num] = strdup(groupname);
                opt->local_servicegroups_num++;
            }
        }
    }

    /* local hostgroups */
    else if (   !strcmp( key, "localhostgroups" )
             || !strcmp( key, "localhostgroup" ) ) {
        char *groupname;
        while ( (groupname = strsep( &value, "," )) != NULL ) {
            groupname = trim(groupname);
            if ( strcmp( groupname, "" ) ) {
                opt->local_hostgroups_list[opt->local_hostgroups_num] = strdup(groupname);
                opt->local_hostgroups_num++;
            }
        }
    }
    return(GM_OK);
}


/* read an entire config file */
int read_config_file(mod_gm_opt_t *opt, char*filename, int recursion_level) {
    FILE * fp;
    int errors = 0;
    char *line;
    char *line_c;

    gm_log( GM_LOG_TRACE, "read_config_file(%s, %d)\n", filename, recursion_level );

    if(recursion_level > 10) {
        gm_log( GM_LOG_ERROR, "deep recursion in config files!\n" );
        return GM_ERROR;
    }
    fp = fopen(filename, "r");
    if(fp == NULL) {
        perror(filename);
        return GM_ERROR;
    }

    line = malloc(GM_BUFFERSIZE);
    line_c = line;
    line[0] = '\0';
    while(fgets(line, GM_BUFFERSIZE, fp) != NULL) {
        /* trim comments */
        int pos = -1;
        if(index(line, '#') != NULL) {
            pos = strcspn(line, "#");
            if(pos == 0)
                continue;
            line[pos] = '\0';
        }
        line = trim(line);
        if(line[0] == 0)
            continue;
        if(parse_args_line(opt, line, recursion_level) != GM_OK) {
            errors++;
            break;
        }
    }
    fclose(fp);
    free(line_c);
    if(errors > 0)
        return(GM_ERROR);
    return(GM_OK);
}

/* dump config */
void dumpconfig(mod_gm_opt_t *opt, int mode) {
    int i=0;
    gm_log( GM_LOG_DEBUG, "--------------------------------\n" );
    gm_log( GM_LOG_DEBUG, "configuration:\n" );
    gm_log( GM_LOG_DEBUG, "log level:           %d\n", opt->debug_level);
    if(mode == GM_WORKER_MODE) {
        gm_log( GM_LOG_DEBUG, "identifier:          %s\n", opt->identifier);
        gm_log( GM_LOG_DEBUG, "pidfile:             %s\n", opt->pidfile == NULL ? "no" : opt->pidfile);
        gm_log( GM_LOG_DEBUG, "logfile:             %s\n", opt->logfile == NULL ? "no" : opt->logfile);
        gm_log( GM_LOG_DEBUG, "job max age:         %d\n", opt->max_age);
        gm_log( GM_LOG_DEBUG, "job timeout:         %d\n", opt->job_timeout);
        gm_log( GM_LOG_DEBUG, "min worker:          %d\n", opt->min_worker);
        gm_log( GM_LOG_DEBUG, "max worker:          %d\n", opt->max_worker);
        gm_log( GM_LOG_DEBUG, "fork on exec:        %s\n", opt->fork_on_exec == GM_ENABLED ? "yes" : "no");
    }
    if(mode == GM_NEB_MODE) {
        gm_log( GM_LOG_DEBUG, "debug result:        %s\n", opt->debug_result == GM_ENABLED ? "yes" : "no");
        gm_log( GM_LOG_DEBUG, "result_worker:       %d\n", opt->result_workers);
    }
    if(mode == GM_NEB_MODE || mode == GM_SEND_GEARMAN_MODE) {
        gm_log( GM_LOG_DEBUG, "result_queue:        %s\n", opt->result_queue);
    }
    gm_log( GM_LOG_DEBUG, "\n" );

    /* server && queues */
    for(i=0;i<opt->server_num;i++)
        gm_log( GM_LOG_DEBUG, "server:              %s\n", opt->server_list[i]);
    gm_log( GM_LOG_DEBUG, "\n" );
    if(mode == GM_NEB_MODE) {
        gm_log( GM_LOG_DEBUG, "perfdata:            %s\n", opt->perfdata     == GM_ENABLED ? "yes" : "no");
    }
    if(mode == GM_NEB_MODE || mode == GM_WORKER_MODE) {
        gm_log( GM_LOG_DEBUG, "hosts:               %s\n", opt->hosts        == GM_ENABLED ? "yes" : "no");
        gm_log( GM_LOG_DEBUG, "services:            %s\n", opt->services     == GM_ENABLED ? "yes" : "no");
        gm_log( GM_LOG_DEBUG, "eventhandler:        %s\n", opt->events       == GM_ENABLED ? "yes" : "no");
        for(i=0;i<opt->hostgroups_num;i++)
            gm_log( GM_LOG_DEBUG, "hostgroups:          %s\n", opt->hostgroups_list[i]);
        for(i=0;i<opt->servicegroups_num;i++)
            gm_log( GM_LOG_DEBUG, "servicegroups:       %s\n", opt->servicegroups_list[i]);
    }

    if(mode == GM_NEB_MODE) {
        for(i=0;i<opt->local_hostgroups_num;i++)
            gm_log( GM_LOG_DEBUG, "local_hostgroups: %s\n", opt->local_hostgroups_list[i]);
        for(i=0;i<opt->local_servicegroups_num;i++)
            gm_log( GM_LOG_DEBUG, "local_servicegroups:      %s\n", opt->local_servicegroups_list[i]);
    }

    /* encryption */
    gm_log( GM_LOG_DEBUG, "\n" );
    gm_log( GM_LOG_DEBUG, "encryption:          %s\n", opt->encryption == GM_ENABLED ? "yes" : "no");
    if(opt->encryption == GM_ENABLED) {
        gm_log( GM_LOG_DEBUG, "keyfile:             %s\n", opt->keyfile == NULL ? "no" : opt->keyfile);
        if(opt->crypt_key != NULL) {
            gm_log( GM_LOG_DEBUG, "encryption key:      set\n" );
        } else {
            gm_log( GM_LOG_DEBUG, "encryption key:      not set\n" );
        }
    }
    gm_log( GM_LOG_DEBUG, "transport mode:      %s\n", opt->encryption == GM_ENABLED ? "aes-256+base64" : "base64 only");

    gm_log( GM_LOG_DEBUG, "--------------------------------\n" );
    return;
}


/* free options structure */
void mod_gm_free_opt(mod_gm_opt_t *opt) {
    int i;
    for(i=0;i<opt->server_num;i++)
        free(opt->server_list[i]);
    for(i=0;i<opt->hostgroups_num;i++)
        free(opt->hostgroups_list[i]);
    for(i=0;i<opt->servicegroups_num;i++)
        free(opt->servicegroups_list[i]);
    for(i=0;i<opt->local_hostgroups_num;i++)
        free(opt->local_hostgroups_list[i]);
    for(i=0;i<opt->local_servicegroups_num;i++)
        free(opt->local_servicegroups_list[i]);
    free(opt->crypt_key);
    free(opt->keyfile);
    free(opt->message);
    free(opt->pidfile);
    free(opt->logfile);
    free(opt->host);
    free(opt->service);
    free(opt->identifier);
    free(opt);
}


/* read keyfile */
int read_keyfile(mod_gm_opt_t *opt) {
    FILE *fp;
    int i;

    if(opt->keyfile == NULL)
        return(GM_ERROR);


    fp = fopen(opt->keyfile,"rb");
    if(fp == NULL) {
        perror(opt->keyfile);
        return(GM_ERROR);
    }
    if(opt->crypt_key != NULL)
        free(opt->crypt_key);
    opt->crypt_key = malloc(GM_BUFFERSIZE);
    for(i=0;i<32;i++)
        opt->crypt_key[i] = fgetc(fp);
    fclose(fp);
    return(GM_OK);
}


/* convert number to signal name */
char * nr2signal(int sig) {
    char * signame = NULL;
    switch(sig) {
        case 1:  signame = "SIGHUP";
                 break;
        case 2:  signame = "SIGINT";
                 break;
        case 3:  signame = "SIGQUIT";
                 break;
        case 4:  signame = "SIGILL";
                 break;
        case 5:  signame = "SIGTRAP";
                 break;
        case 6:  signame = "SIGABRT";
                 break;
        case 7:  signame = "SIGBUS";
                 break;
        case 8:  signame = "SIGFPE";
                 break;
        case 9:  signame = "SIGKILL";
                 break;
        case 10: signame = "SIGUSR1";
                 break;
        case 11: signame = "SIGSEGV";
                 break;
        case 12: signame = "SIGUSR2";
                 break;
        case 13: signame = "SIGPIPE";
                 break;
        case 14: signame = "SIGALRM";
                 break;
        case 15: signame = "SIGTERM";
                 break;
        case 16: signame = "SIGURG";
                 break;
        default: snprintf(signame, 20, "signal %d", sig);
                 break;
    }
    return strdup(signame);
}


/* convert to time */
void string2timeval(char * value, struct timeval *t) {
    char * v;
    char * v_c;
    char * s;
    char * u;

    t->tv_sec  = 0;
    t->tv_usec = 0;

    if(value == NULL)
        return;

    v = strdup(value);
    v_c = v;

    s = strsep( &v, "." );
    if(s == NULL) {
        free(v_c);
        return;
    }

    t->tv_sec  = strtoul(s,NULL,0);

    u = strsep( &v, "\x0" );

    if(u == NULL) {
        free(v_c);
        return;
    }

    t->tv_usec = strtoul(u,NULL,0);
    free(v_c);
}

/* convert a timeval to double */
double timeval2double(struct timeval * t) {
    double val = 0.0;
    if(t != NULL) {
        val = (double)t->tv_sec + ((double)t->tv_usec / 1000000);
    }
    return val;
}


/* compare 2 timestructs */
long mod_gm_time_compare(struct timeval * tv1, struct timeval * tv2) {
   long secdiff = (long)(tv1->tv_sec - tv2->tv_sec);
   long usecdiff = (long)(tv1->tv_usec - tv2->tv_usec);
   return secdiff? secdiff: usecdiff;
}


/* extract check result */
char *extract_check_result(FILE *fp) {
    int size;
    char buffer[GM_BUFFERSIZE] = "";
    char output[GM_BUFFERSIZE] = "";

    /* get all lines of plugin output - escape newlines */
    strcpy(buffer,"");
    strcpy(output,"");
    size = GM_MAX_OUTPUT;
    while(size > 0 && fgets(buffer,sizeof(buffer)-1,fp)){
        strncat(output, buffer, size);
        size -= strlen(buffer);
    }
    return(escape_newlines(output));
}


/* convert a command line to an array of arguments, suitable for exec* functions */
int parse_command_line(char *cmd, char *argv[MAX_CMD_ARGS]) {
    unsigned int argc=0;
    char *parsed_cmd;

    /* Skip initial white-space characters. */
    for(parsed_cmd=cmd;isspace(*cmd);++cmd)
        ;

    /* Parse command line. */
    while(*cmd&&(argc<MAX_CMD_ARGS-1)){
        argv[argc++]=parsed_cmd;
        switch(*cmd){
        case '\'':
            while((*cmd)&&(*cmd!='\''))
                *(parsed_cmd++)=*(cmd++);
            if(*cmd)
                ++cmd;
            break;
        case '"':
            while((*cmd)&&(*cmd!='"')){
                if((*cmd=='\\')&&cmd[1]&&strchr("\"\\\n",cmd[1]))
                    ++cmd;
                *(parsed_cmd++)=*(cmd++);
                }
            if(*cmd)
                ++cmd;
            break;
        default:
            while((*cmd)&&!isspace(*cmd)){
                if((*cmd=='\\')&&cmd[1])
                    ++cmd;
                *(parsed_cmd++)=*(cmd++);
                }
            }
        while(isspace(*cmd))
            ++cmd;
        *(parsed_cmd++)='\0';
        }
    argv[argc]=NULL;

    return GM_OK;
}


/* run a check */
int run_check(char *processed_command, char **ret) {
    char *argv[MAX_CMD_ARGS];
    FILE *fp;
    pid_t pid;
    int pipefds[2];
    int retval;

    /* check for check execution method (shell or execvp) */
    if(!strpbrk(processed_command,"!$^&*()~[]|{};<>?`\"'")) {
        gm_log( GM_LOG_TRACE, "using execvp\n" );
        if(pipe(pipefds)) {
            gm_log( GM_LOG_ERROR, "error creating pipe: %s\n", strerror(errno));
            _exit(STATE_UNKNOWN);
        }
        if((pid=fork())<0){
            gm_log( GM_LOG_ERROR, "fork error\n");
            _exit(STATE_UNKNOWN);
        }
        else if(!pid){
            if((dup2(pipefds[1],STDOUT_FILENO)<0)||(dup2(pipefds[1],STDERR_FILENO)<0)){
                gm_log( GM_LOG_ERROR, "dup2 error\n");
                _exit(STATE_UNKNOWN);
            }
            close(pipefds[1]);
            parse_command_line(processed_command,argv);
            if(!argv[0])
                _exit(STATE_UNKNOWN);
            execvp(argv[0], argv);
            _exit(STATE_UNKNOWN);
        }

        /* prepare pipe reading */
        close(pipefds[1]);
        fp=fdopen(pipefds[0],"r");
        if(!fp){
            gm_log( GM_LOG_ERROR, "fdopen error\n");
            _exit(STATE_UNKNOWN);
        }

        /* extract check result */
        *ret = extract_check_result(fp);

        /* close the process */
        fclose(fp);
        close(pipefds[0]);
        if(waitpid(pid,&retval,0)!=pid)
            retval=-1;
    }
    else {
        gm_log( GM_LOG_TRACE, "using popen\n" );
        fp=popen(processed_command,"r");
        if(fp==NULL)
            _exit(STATE_UNKNOWN);

        /* extract check result */
        *ret = extract_check_result(fp);


        /* close the process */
        retval=pclose(fp);
     }

    return retval;
}
