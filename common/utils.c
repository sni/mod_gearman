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

#include "config.h"
#include "utils.h"
#include "gm_crypt.h"
#include "base64.h"
#include "gearman_utils.h"
#include "popenRWE.h"
#include "polarssl/md5.h"

#ifdef EMBEDDEDPERL
#include "epn_utils.h"
int enable_embedded_perl         = GM_ENABLED;
int use_embedded_perl_implicitly = GM_DISABLED;
int use_perl_cache               = GM_ENABLED;
char *p1_file                    = NULL;
#endif

/* escapes newlines in a string */
char *gm_escape_newlines(char *rawbuf, int trimmed) {
    char *tmpbuf=NULL;
    char *tmpbuf_dup=NULL;
    char *newbuf=NULL;
    register int x,y;

    if(rawbuf==NULL)
        return NULL;

    tmpbuf     = gm_strdup(rawbuf);
    tmpbuf_dup = tmpbuf;
    if ( trimmed == GM_ENABLED ) {
        tmpbuf = trim(tmpbuf);
    }

    /* allocate enough memory to escape all chars if necessary */
    if((newbuf=gm_malloc((strlen(tmpbuf)*2)+1))==NULL) {
        free(tmpbuf);
        return NULL;
    }

    for(x=0,y=0;tmpbuf[x]!=(char)'\x0';x++){

        /* escape backslashes */
        if(tmpbuf[x]=='\\'){
            newbuf[y++]='\\';
            newbuf[y++]='\\';
        }

        /* escape newlines */
        else if(tmpbuf[x]=='\n'){
            newbuf[y++]='\\';
            newbuf[y++]='n';
        }

        else
            newbuf[y++]=tmpbuf[x];
    }

    newbuf[y]='\x0';

    free(tmpbuf_dup);

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
        crypted = (unsigned char*)gm_strdup(text);
        size    = strlen(text);
    }

    /* now encode in base64 */
    base64 = gm_malloc(size*2);
    base64[0] = 0;
    base64_encode(crypted, size, base64, size*2);
    free(crypted);
    *encrypted = base64;
    return strlen(base64);
}


/* decrypt text with given key */
void mod_gm_decrypt(char ** decrypted, char * text, int mode) {
    char *test;
    int input_size = strlen(text);
    unsigned char * buffer = gm_malloc(sizeof(unsigned char) * input_size * 2);

    /* first decode from base64 */
    size_t bsize = base64_decode(text, buffer, input_size);
    test = gm_strndup((char *)buffer, 5);
    if(mode == GM_ENCODE_AND_ENCRYPT || (mode == GM_ENCODE_ACCEPT_ALL && strcmp(test, "type="))) {
        /* then decrypt */
        mod_gm_aes_decrypt(decrypted, buffer, bsize);
    }
    else  {
        *decrypted[0] = '\x0';
        buffer[bsize] = '\x0';
        strncat(*decrypted, (char*)buffer, bsize);
    }
    free(test);
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
    if(strlen(s) == 0)
        return s;
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
    opt->logmode            = GM_LOG_MODE_AUTO;
    opt->logfile_fp         = NULL;
    opt->message            = NULL;
    opt->delimiter          = gm_strdup("\t");
    opt->return_code        = 0;
    opt->timeout            = 10;
    opt->debug_level        = GM_LOG_INFO;
    opt->perfdata           = GM_DISABLED;
    opt->perfdata_mode      = GM_PERFDATA_OVERWRITE;
    opt->perfdata_send_all  = GM_DISABLED;
    opt->use_uniq_jobs      = GM_ENABLED;
    opt->do_hostchecks      = GM_ENABLED;
    opt->route_eventhandler_like_checks = GM_DISABLED;
    opt->hosts              = GM_DISABLED;
    opt->services           = GM_DISABLED;
    opt->events             = GM_DISABLED;
    opt->notifications      = GM_DISABLED;
    opt->job_timeout        = GM_DEFAULT_JOB_TIMEOUT;
    opt->encryption         = GM_ENABLED;
    opt->pidfile            = NULL;
    opt->debug_result       = GM_DISABLED;
    opt->max_age            = GM_DEFAULT_JOB_MAX_AGE;
    opt->min_worker         = GM_DEFAULT_MIN_WORKER;
    opt->max_worker         = GM_DEFAULT_MAX_WORKER;
    opt->transportmode      = GM_ENCODE_AND_ENCRYPT;
    opt->daemon_mode        = GM_DISABLED;
    opt->fork_on_exec       = GM_DISABLED;
    opt->idle_timeout       = GM_DEFAULT_IDLE_TIMEOUT;
    opt->max_jobs           = GM_DEFAULT_MAX_JOBS;
    opt->spawn_rate         = GM_DEFAULT_SPAWN_RATE;
    opt->timeout_return     = 2;
    opt->identifier         = NULL;
    opt->queue_cust_var     = NULL;
    opt->show_error_output  = GM_ENABLED;
    opt->dup_results_are_passive = GM_ENABLED;
    opt->orphan_host_checks      = GM_ENABLED;
    opt->orphan_service_checks   = GM_ENABLED;
    opt->orphan_return           = 2;
    opt->accept_clear_results    = GM_DISABLED;
    opt->has_starttime      = FALSE;
    opt->has_finishtime     = FALSE;
    opt->has_latency        = FALSE;
    opt->active             = GM_DISABLED;

    opt->restrict_command_characters = gm_strdup("$&();<>`\"'|");
    opt->workaround_rc_25            = GM_DISABLED;

    opt->host               = NULL;
    opt->service            = NULL;

#ifdef EMBEDDEDPERL
    opt->enable_embedded_perl         = GM_ENABLED;
    opt->use_embedded_perl_implicitly = GM_DISABLED;
    opt->use_perl_cache               = GM_ENABLED;
    opt->p1_file                      = NULL;
#endif

    opt->server_num         = 0;
    for(i=0;i<GM_LISTSIZE;i++)
        opt->server_list[i] = NULL;
    opt->dupserver_num         = 0;
    for(i=0;i<GM_LISTSIZE;i++)
        opt->dupserver_list[i] = NULL;
    opt->perfdata_queues_num = 0;
    for(i=0;i<GM_LISTSIZE;i++)
        opt->perfdata_queues_list[i] = NULL;
    opt->hostgroups_num     = 0;
    for(i=0;i<GM_LISTSIZE;i++)
        opt->hostgroups_list[i] = NULL;
    opt->servicegroups_num  = 0;
    for(i=0;i<GM_LISTSIZE;i++)
        opt->servicegroups_list[i] = NULL;
    opt->local_hostgroups_num     = 0;
    for(i=0;i<GM_LISTSIZE;i++)
        opt->local_hostgroups_list[i] = NULL;
    opt->local_servicegroups_num  = 0;
    for(i=0;i<GM_LISTSIZE;i++)
        opt->local_servicegroups_list[i] = NULL;
    for(i=0;i<GM_NEBTYPESSIZE;i++) {
        mod_gm_exp_t *mod_gm_exp;
        mod_gm_exp              = gm_malloc(sizeof(mod_gm_exp_t));
        mod_gm_exp->elem_number = 0;
        opt->exports[i]         = mod_gm_exp;
    }
    opt->exports_count = 0;
    opt->restrict_path_num      = 0;
    opt->gearman_connection_timeout = -1;
    for(i=0;i<GM_LISTSIZE;i++)
        opt->restrict_path[i] = NULL;

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
    int i, x, number, callback_num;
    char *key;
    char *value;
    char *callback;
    char *export_queue;
    char *return_code;
    int return_code_num;
    char *callbacks;
    char * type;

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

    /* daemon mode or delimiter */
    if ( !strcmp( key, "daemon" ) ||  !strcmp( key, "d" ) ) {
        opt->daemon_mode = parse_yes_or_no(value, GM_ENABLED);
        if(value != NULL) {
            free(opt->delimiter);
            opt->delimiter = gm_strdup( value );
        }
        return(GM_OK);
    }

    /* perfdata */
    else if ( !strcmp(key, "perfdata") && strlen(value) ) {
        opt->set_queues_by_hand++;
        opt->perfdata = parse_yes_or_no(value, -1);

        if (opt->perfdata == GM_ENABLED) {
            //bool, use default queue
            opt->perfdata_queues_list[opt->perfdata_queues_num] = gm_strdup(GM_PERFDATA_QUEUE);
            opt->perfdata_queues_num++;
        }
        else if (opt->perfdata == -1) {
            //not a bool, use value string as queue name(s)
            opt->perfdata = GM_ENABLED;

            char *values, *values_original;
            values_original = values = gm_strdup(value);
            char *name;
            while ((name = strsep(&values, ",")) != NULL) {
                opt->perfdata_queues_list[opt->perfdata_queues_num] = gm_strdup(name);
                opt->perfdata_queues_num++;
            }
            free(values_original);
        }
        return(GM_OK);
    }

    /* perfdata_send_all */
    else if ( !strcmp(key, "perfdata_send_all") ) {
        /* perfdata override to dump all performance values */
        opt->perfdata_send_all = parse_yes_or_no(value, GM_DISABLED);
        return(GM_OK);
    }

    /* hosts */
    else if ( !strcmp( key, "hosts" ) ) {
        opt->set_queues_by_hand++;
        opt->hosts = parse_yes_or_no(value, GM_ENABLED);
        return(GM_OK);
    }

    /* services */
    else if ( !strcmp( key, "services" ) ) {
        opt->set_queues_by_hand++;
        opt->services = parse_yes_or_no(value, GM_ENABLED);
        return(GM_OK);
    }

    /* eventhandler */
    else if (   !strcmp( key, "eventhandlers" )
             || !strcmp( key, "eventhandler" )
            ) {
        opt->set_queues_by_hand++;
        opt->events = parse_yes_or_no(value, GM_ENABLED);
        return(GM_OK);
    }

    /* notifications */
    else if (   !strcmp( key, "notifications" )
             || !strcmp( key, "notification" )
            ) {
        opt->set_queues_by_hand++;
        opt->notifications = parse_yes_or_no(value, GM_ENABLED);
        return(GM_OK);
    }

    /* debug-result */
    else if (   !strcmp( key, "debug-result" )
             || !strcmp( key, "debug_result" )
            ) {
        opt->debug_result = parse_yes_or_no(value, GM_ENABLED);
        return(GM_OK);
    }

    /* encryption */
    else if ( !strcmp( key, "encryption" ) ) {
        opt->encryption = parse_yes_or_no(value, GM_ENABLED);
        return(GM_OK);
    }

    /* fork_on_exec */
    else if ( !strcmp( key, "fork_on_exec" ) ) {
        opt->fork_on_exec = parse_yes_or_no(value, GM_ENABLED);
        return(GM_OK);
    }

    /* do_hostchecks */
    else if ( !strcmp( key, "do_hostchecks" ) ) {
        opt->do_hostchecks = parse_yes_or_no(value, GM_ENABLED);
        return(GM_OK);
    }

    /* route_eventhandler_like_checks */
    else if ( !strcmp( key, "route_eventhandler_like_checks" ) ) {
        opt->route_eventhandler_like_checks = parse_yes_or_no(value, GM_ENABLED);
        return(GM_OK);
    }

    /* active */
    else if ( !strcmp( key, "active" ) ) {
        opt->active = parse_yes_or_no(value, GM_ENABLED);
        return(GM_OK);
    }

    /* show_error_output */
    else if ( !strcmp( key, "show_error_output" ) ) {
        opt->show_error_output = parse_yes_or_no(value, GM_ENABLED);
        return(GM_OK);
    }

    /* dup_results_are_passive */
    else if ( !strcmp( key, "dup_results_are_passive" ) ) {
        opt->dup_results_are_passive = parse_yes_or_no(value, GM_ENABLED);
        return(GM_OK);
    }

    /* workaround_rc_25 */
    else if ( !strcmp( key, "workaround_rc_25" ) ) {
        opt->workaround_rc_25 = parse_yes_or_no(value, GM_ENABLED);
        return(GM_OK);
    }

    /* orphan_host_checks */
    else if ( !strcmp( key, "orphan_host_checks" ) ) {
        opt->orphan_host_checks = parse_yes_or_no(value, GM_ENABLED);
        return(GM_OK);
    }

    /* orphan_service_checks */
    else if ( !strcmp( key, "orphan_service_checks" ) ) {
        opt->orphan_service_checks = parse_yes_or_no(value, GM_ENABLED);
        return(GM_OK);
    }

    /* accept_clear_results */
    else if ( !strcmp( key, "accept_clear_results" ) ) {
        opt->accept_clear_results = parse_yes_or_no(value, GM_ENABLED);
        return(GM_OK);
    }

    /* enable_embedded_perl */
    else if ( !strcmp( key, "enable_embedded_perl" ) ) {
#ifdef EMBEDDEDPERL
        opt->enable_embedded_perl = parse_yes_or_no(value, GM_ENABLED);
        enable_embedded_perl = opt->enable_embedded_perl;
#endif
        return(GM_OK);
    }
    /* use_embedded_perl_implicitly */
    else if ( !strcmp( key, "use_embedded_perl_implicitly" ) ) {
#ifdef EMBEDDEDPERL
        opt->use_embedded_perl_implicitly = parse_yes_or_no(value, GM_ENABLED);
        use_embedded_perl_implicitly = opt->use_embedded_perl_implicitly;
#endif
        return(GM_OK);
    }
    /* use_perl_cache */
    else if ( !strcmp( key, "use_perl_cache" ) ) {
#ifdef EMBEDDEDPERL
        opt->use_perl_cache = parse_yes_or_no(value, GM_ENABLED);
        use_perl_cache = opt->use_perl_cache;
#endif
        return(GM_OK);
    }

    /* use_uniq_jobs */
    else if ( !strcmp( key, "use_uniq_jobs" ) ) {
        opt->use_uniq_jobs = parse_yes_or_no(value, GM_ENABLED);
        return(GM_OK);
    }

    else if ( value == NULL ) {
        gm_log( GM_LOG_ERROR, "unknown switch '%s'\n", key );
        return(GM_OK);
    }

    /* debug */
    if ( !strcmp( key, "debug" ) ) {
        opt->debug_level = atoi( value );
        if(opt->debug_level < 0) { opt->debug_level = 0; }
    }

    /* logmode */
    else if ( !strcmp( key, "logmode" ) ) {
        opt->logmode = GM_LOG_MODE_AUTO;
        if ( !strcmp( value, "automatic" ) ) {
            opt->logmode = GM_LOG_MODE_AUTO;
        }
        else if ( !strcmp( value, "file" ) ) {
            opt->logmode = GM_LOG_MODE_FILE;
        }
        else if ( !strcmp( value, "stdout" ) ) {
            opt->logmode = GM_LOG_MODE_STDOUT;
        }
        else if ( !strcmp( value, "syslog" ) ) {
            opt->logmode = GM_LOG_MODE_SYSLOG;
        }
        else if ( !strcmp( value, "core" ) ) {
            opt->logmode = GM_LOG_MODE_CORE;
        }
        else {
            gm_log( GM_LOG_ERROR, "unknown log mode '%s', use one of 'automatic', 'file', 'stdout', 'syslog' and 'core'\n", value );
        }
    }

    /* result worker */
    else if ( !strcmp( key, "result_workers" ) ) {
        opt->result_workers = atoi( value );
        if(opt->result_workers > 1) { opt->result_workers = 1; }
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
        opt->result_queue = gm_strdup( value );
    }

    /* message */
    else if (   !strcmp( key, "message" )
             || !strcmp( key, "m" )
            ) {
        opt->message = gm_strdup( value );
    }

    /* delimiter */
    else if (   !strcmp( key, "delimiter" ) ) {
        free(opt->delimiter);
        opt->delimiter = gm_strdup( value );
    }

    /* host */
    else if (   !strcmp( key, "host" ) ) {
        opt->host = gm_strdup( value );
    }

    /* service */
    else if (   !strcmp( key, "service" ) ) {
        opt->service = gm_strdup( value );
    }

    /* latency */
    else if ( !strcmp( key, "latency" ) ) {
        opt->has_latency = TRUE;
        string2timeval(value, &opt->latency);
    }

    /* start time */
    else if ( !strcmp( key, "starttime" ) ) {
        opt->has_starttime = TRUE;
        string2timeval(value, &opt->starttime);
    }

    /* finish time */
    else if ( !strcmp( key, "finishtime" ) ) {
        opt->has_finishtime = TRUE;
        string2timeval(value, &opt->finishtime);
    }

    /* configfile / includes */
    else if (   !strcmp( key, "config" )
             || !strcmp( key, "configfile" )
             || !strcmp( key, "include" )
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
        opt->crypt_key = gm_strdup( value );
    }

    /* keyfile / passwordfile */
    else if (   !strcmp( key, "keyfile" )
             || !strcmp( key, "passwordfile" )
            ) {
        opt->keyfile = gm_strdup( value );
    }

    /* pidfile */
    else if ( !strcmp( key, "pidfile" ) ) {
        opt->pidfile = gm_strdup( value );
    }

    /* logfile */
    else if ( !strcmp( key, "logfile" ) ) {
        opt->logfile = gm_strdup( value );
    }

    /* identifier */
    else if ( !strcmp( key, "identifier" ) ) {
        opt->identifier = gm_strdup( value );
    }

    /* timeout */
    else if (   !strcmp( key, "timeout" )
             || !strcmp( key, "t" )) {
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
        if(opt->max_age < 0) { opt->max_age = GM_DEFAULT_JOB_MAX_AGE; }
    }

    /* idle-timeout */
    else if ( !strcmp( key, "idle-timeout" ) ) {
        opt->idle_timeout = atoi( value );
        if(opt->idle_timeout < 0) { opt->idle_timeout = GM_DEFAULT_IDLE_TIMEOUT; }
    }

    /* max-jobs */
    else if ( !strcmp( key, "max-jobs" ) ) {
        opt->max_jobs = atoi( value );
        if(opt->max_jobs < 0) { opt->max_jobs = GM_DEFAULT_MAX_JOBS; }
    }

    /* spawn-rate */
    else if ( !strcmp( key, "spawn-rate" ) ) {
        opt->spawn_rate = atoi( value );
        if(opt->spawn_rate < 0) { opt->spawn_rate = GM_DEFAULT_SPAWN_RATE; }
    }

    /* load limit 1min */
    else if ( !strcmp( key, "load_limit1" ) ) {
        opt->load_limit1 = atof( value );
        if(opt->load_limit1 < 0) { opt->load_limit1 = 0; }
    }
    /* load limit 5min */
    else if ( !strcmp( key, "load_limit5" ) ) {
        opt->load_limit5 = atof( value );
        if(opt->load_limit5 < 0) { opt->load_limit5 = 0; }
    }
    /* load limit 15min */
    else if ( !strcmp( key, "load_limit15" ) ) {
        opt->load_limit15 = atof( value );
        if(opt->load_limit15 < 0) { opt->load_limit15 = 0; }
    }

    /* timeout_return */
    else if ( !strcmp( key, "timeout_return" ) ) {
        opt->timeout_return = atoi( value );
        if(opt->timeout_return < 0 || opt->timeout_return > 3) {
            gm_log( GM_LOG_INFO, "Warning: unknown timeout_return: %d\n", opt->timeout_return );
            opt->timeout_return = 2;
        }
    }

    /* orphan_return */
    else if ( !strcmp( key, "orphan_return" ) ) {
        opt->orphan_return = atoi( value );
        if(opt->orphan_return < 0 || opt->orphan_return > 3) {
            gm_log( GM_LOG_INFO, "Warning: unknown orphan_return: %d\n", opt->orphan_return );
            opt->orphan_return = 2;
        }
    }

    /* perfdata_mode */
    else if ( !strcmp( key, "perfdata_mode" ) ) {
        opt->perfdata_mode = atoi( value );
        if(opt->perfdata_mode < 0 || opt->perfdata_mode > 2) {
            gm_log( GM_LOG_INFO, "Warning: unknown perfdata_mode: %d\n", opt->perfdata_mode );
            opt->perfdata_mode = GM_PERFDATA_OVERWRITE;
        }
    }

    /* server */
    else if ( !strcmp( key, "server" ) ) {
        char *servername;
        while ( (servername = strsep( &value, "," )) != NULL ) {
            add_server(&opt->server_num, opt->server_list, servername);
        }
    }

    /* duplicate server */
    else if ( !strcmp( key, "dupserver" ) ) {
        char *servername;
        while ( (servername = strsep( &value, "," )) != NULL ) {
            add_server(&opt->dupserver_num, opt->dupserver_list, servername);
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
                if(strlen(groupname) > 50) {
                    gm_log( GM_LOG_ERROR, "servicegroup name '%s' is too long, please use a maximum of 50 characters\n", groupname );
                } else {
                    opt->servicegroups_list[opt->servicegroups_num] = gm_strdup(groupname);
                    opt->servicegroups_num++;
                    opt->set_queues_by_hand++;
                }
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
                if(strlen(groupname) > 50) {
                    gm_log( GM_LOG_ERROR, "hostgroup name '%s' is too long, please use a maximum of 50 characters\n", groupname );
                } else {
                    opt->hostgroups_list[opt->hostgroups_num] = gm_strdup(groupname);
                    opt->hostgroups_num++;
                    opt->set_queues_by_hand++;
                }
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
                opt->local_servicegroups_list[opt->local_servicegroups_num] = gm_strdup(groupname);
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
                opt->local_hostgroups_list[opt->local_hostgroups_num] = gm_strdup(groupname);
                opt->local_hostgroups_num++;
            }
        }
    }

    /* queue_custom_variable */
    else if ( !strcmp( key, "queue_custom_variable" ) ) {
        /* uppercase custom variable name */
        for(x = 0; value[x] != '\x0'; x++) {
            value[x] = toupper(value[x]);
        }
        opt->queue_cust_var = gm_strdup( value );
    }

    /* export queues */
    else if ( !strcmp( key, "export" ) ) {
        export_queue        = strsep( &value, ":" );
        export_queue        = trim(export_queue);
        return_code         = strsep( &value, ":" );
        return_code_num     = atoi(return_code);
        callbacks           = strsep( &value, ":" );
        if(strlen(export_queue) > 50) {
            gm_log( GM_LOG_ERROR, "export queue name '%s' is too long, please use a maximum of 50 characters\n", export_queue );
        } else {
            while ( (callback = strsep( &callbacks, "," )) != NULL ) {
                callback_num = atoi(trim(callback));
                if(index(callback, 'N') != NULL) {
                    callback_num = -1;
                    /* get neb callback number by name */
                    for(i=0;i<GM_NEBTYPESSIZE;i++) {
                        type = nebcallback2str(i);
                        if(!strcmp(type, callback)) {
                            callback_num = i;
                        }
                        free(type);
                    }
                    if(callback_num == -1) {
                        gm_log( GM_LOG_ERROR, "unknown nebcallback : %s\n", callback);
                        continue;
                    }
                }

                number = opt->exports[callback_num]->elem_number;
                opt->exports[callback_num]->name[number]        = gm_strdup(export_queue);
                opt->exports[callback_num]->return_code[number] = return_code_num;
                opt->exports[callback_num]->elem_number++;
            }
            opt->exports_count++;
        }
    }

    /* p1_file */
    else if ( !strcmp( key, "p1_file" ) ) {
#ifdef EMBEDDEDPERL
        opt->p1_file = gm_strdup( value );
        free(p1_file);
        p1_file = gm_strdup(opt->p1_file);
#endif
    }

    /* restrict_path */
    else if ( !strcmp( key, "restrict_path" ) || !strcmp( key, "restrictpath" )) {
        opt->restrict_path[opt->restrict_path_num] = gm_strdup(value);
        opt->restrict_path_num++;
    }

    /* restrict_command_characters */
    else if ( !strcmp( key, "restrict_command_characters") ) {
        free(opt->restrict_command_characters);
        opt->restrict_command_characters = gm_strdup(value);
    }

    /* timeout while connecting to gearmand server*/
    else if ( !strcmp( key, "gearman_connection_timeout" ) ) {
        opt->gearman_connection_timeout = atoi( value );
    }

    else {
        gm_log( GM_LOG_ERROR, "unknown option '%s'\n", key );
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

    line = gm_malloc(GM_BUFFERSIZE);
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
    int j=0;
    gm_log( GM_LOG_DEBUG, "--------------------------------\n" );
    gm_log( GM_LOG_DEBUG, "configuration:\n" );
    gm_log( GM_LOG_DEBUG, "log level:                       %d\n", opt->debug_level);

    if(opt->logmode == GM_LOG_MODE_AUTO)
        gm_log( GM_LOG_DEBUG, "log mode:                        auto (%d)\n", opt->logmode);
    if(opt->logmode == GM_LOG_MODE_FILE)
        gm_log( GM_LOG_DEBUG, "log mode:                        file (%d)\n", opt->logmode);
    if(opt->logmode == GM_LOG_MODE_STDOUT)
        gm_log( GM_LOG_DEBUG, "log mode:                        stdout (%d)\n", opt->logmode);
    if(opt->logmode == GM_LOG_MODE_CORE)
        gm_log( GM_LOG_DEBUG, "log mode:                        core (%d)\n", opt->logmode);
    if(opt->logmode == GM_LOG_MODE_SYSLOG)
        gm_log( GM_LOG_DEBUG, "log mode:                        syslog (%d)\n", opt->logmode);
    if(opt->logmode == GM_LOG_MODE_TOOLS)
        gm_log( GM_LOG_DEBUG, "log mode:                        tools (%d)\n", opt->logmode);

    if(mode == GM_WORKER_MODE) {
        gm_log( GM_LOG_DEBUG, "identifier:                      %s\n", opt->identifier);
        gm_log( GM_LOG_DEBUG, "pidfile:                         %s\n", opt->pidfile == NULL ? "no" : opt->pidfile);
        gm_log( GM_LOG_DEBUG, "logfile:                         %s\n", opt->logfile == NULL ? "no" : opt->logfile);
        gm_log( GM_LOG_DEBUG, "job max num:                     %d\n", opt->max_jobs);
        gm_log( GM_LOG_DEBUG, "job max age:                     %d\n", opt->max_age);
        gm_log( GM_LOG_DEBUG, "job timeout:                     %d\n", opt->job_timeout);
        gm_log( GM_LOG_DEBUG, "min worker:                      %d\n", opt->min_worker);
        gm_log( GM_LOG_DEBUG, "max worker:                      %d\n", opt->max_worker);
        gm_log( GM_LOG_DEBUG, "spawn rate:                      %d\n", opt->spawn_rate);
        gm_log( GM_LOG_DEBUG, "fork on exec:                    %s\n", opt->fork_on_exec == GM_ENABLED ? "yes" : "no");
#ifndef EMBEDDEDPERL
        gm_log( GM_LOG_DEBUG, "embedded perl:                   not compiled\n");
#endif
#ifdef EMBEDDEDPERL
        gm_log( GM_LOG_DEBUG, "\n" );
        gm_log( GM_LOG_DEBUG, "embedded perl:                   %s\n", opt->enable_embedded_perl == GM_ENABLED ? "yes" : "no");
        gm_log( GM_LOG_DEBUG, "use_epn_implicitly:              %s\n", opt->use_embedded_perl_implicitly == GM_ENABLED ? "yes" : "no");
        gm_log( GM_LOG_DEBUG, "use_perl_cache:                  %s\n", opt->use_perl_cache == GM_ENABLED ? "yes" : "no");
        gm_log( GM_LOG_DEBUG, "p1_file:                         %s\n", opt->p1_file == NULL ? "not set" : opt->p1_file );
        for(i=0;i<opt->restrict_path_num;i++)
            gm_log( GM_LOG_DEBUG, "restricted path:                 %s\n", opt->restrict_path[i]);
        if(opt->restrict_path_num > 0)
            gm_log( GM_LOG_DEBUG, "restrict_command_characters:     %s\n", opt->restrict_command_characters);
#endif
    }
    if(mode == GM_NEB_MODE) {
        gm_log( GM_LOG_DEBUG, "queue by cust var:               %s\n", opt->queue_cust_var == NULL ? "no" : opt->queue_cust_var);
        gm_log( GM_LOG_DEBUG, "debug result:                    %s\n", opt->debug_result == GM_ENABLED ? "yes" : "no");
        if(opt->result_workers != 1)
            gm_log( GM_LOG_DEBUG, "result_worker:                   %d\n", opt->result_workers);
        gm_log( GM_LOG_DEBUG, "do_hostchecks:                   %s\n", opt->do_hostchecks == GM_ENABLED ? "yes" : "no");
        gm_log( GM_LOG_DEBUG, "route_eventhandler_like_checks:  %s\n", opt->route_eventhandler_like_checks == GM_ENABLED ? "yes" : "no");
    }
    if(mode == GM_NEB_MODE || mode == GM_SEND_GEARMAN_MODE) {
        gm_log( GM_LOG_DEBUG, "result_queue:                    %s\n", opt->result_queue);
    }
    gm_log( GM_LOG_DEBUG, "\n" );

    /* server && queues */
    for(i=0;i<opt->server_num;i++)
        gm_log( GM_LOG_DEBUG, "server:                          %s:%i\n", opt->server_list[i]->host, opt->server_list[i]->port);
    gm_log( GM_LOG_DEBUG, "\n" );
    for(i=0;i<opt->dupserver_num;i++)
        gm_log( GM_LOG_DEBUG, "dupserver:                       %s:%i\n", opt->dupserver_list[i]->host, opt->dupserver_list[i]->port);
    gm_log( GM_LOG_DEBUG, "\n" );
    if(mode == GM_NEB_MODE) {
        gm_log( GM_LOG_DEBUG, "perfdata:                        %s\n", opt->perfdata      == GM_ENABLED ? "yes" : "no");
        gm_log( GM_LOG_DEBUG, "perfdata mode:                   %s\n", opt->perfdata_mode == GM_PERFDATA_OVERWRITE ? "overwrite" : "append");
    }
    if(mode == GM_NEB_MODE || mode == GM_WORKER_MODE) {
        gm_log( GM_LOG_DEBUG, "hosts:                           %s\n", opt->hosts         == GM_ENABLED ? "yes" : "no");
        gm_log( GM_LOG_DEBUG, "services:                        %s\n", opt->services      == GM_ENABLED ? "yes" : "no");
        gm_log( GM_LOG_DEBUG, "eventhandler:                    %s\n", opt->events        == GM_ENABLED ? "yes" : "no");
        gm_log( GM_LOG_DEBUG, "notifications:                   %s\n", opt->notifications == GM_ENABLED ? "yes" : "no");
        for(i=0;i<opt->hostgroups_num;i++)
            gm_log( GM_LOG_DEBUG, "hostgroups:                      %s\n", opt->hostgroups_list[i]);
        for(i=0;i<opt->servicegroups_num;i++)
            gm_log( GM_LOG_DEBUG, "servicegroups:                   %s\n", opt->servicegroups_list[i]);
    }

    if(mode == GM_NEB_MODE) {
        for(i=0;i<opt->local_hostgroups_num;i++)
            gm_log( GM_LOG_DEBUG, "local_hostgroups:                %s\n", opt->local_hostgroups_list[i]);
        for(i=0;i<opt->local_servicegroups_num;i++)
            gm_log( GM_LOG_DEBUG, "local_servicegroups:             %s\n", opt->local_servicegroups_list[i]);
        /* export queues*/
        for(i=0;i<GM_NEBTYPESSIZE;i++) {
            char * type = nebcallback2str(i);
            for(j=0;j<opt->exports[i]->elem_number;j++)
                gm_log( GM_LOG_DEBUG, "export:                          %-45s -> %s\n", type, opt->exports[i]->name[j]);
            free(type);
        }
    }

    /* encryption */
    gm_log( GM_LOG_DEBUG, "\n" );
    gm_log( GM_LOG_DEBUG, "encryption:                      %s\n", opt->encryption == GM_ENABLED ? "yes" : "no");
    if(opt->encryption == GM_ENABLED) {
        gm_log( GM_LOG_DEBUG, "keyfile:                         %s\n", opt->keyfile == NULL ? "no" : opt->keyfile);
        if(opt->crypt_key != NULL) {
            gm_log( GM_LOG_DEBUG, "encryption key:                  set\n" );
        } else {
            gm_log( GM_LOG_DEBUG, "encryption key:                  not set\n" );
        }
    }
    if(mode == GM_NEB_MODE) {
        gm_log( GM_LOG_DEBUG, "accept clear result:             %s\n", opt->accept_clear_results == GM_ENABLED ? "yes" : "no");
    }
    gm_log( GM_LOG_DEBUG, "transport mode:                  %s\n", opt->encryption == GM_ENABLED ? "aes-256+base64" : "base64 only");
    gm_log( GM_LOG_DEBUG, "use uniq jobs:                   %s\n", opt->use_uniq_jobs == GM_ENABLED ? "yes" : "no");

    gm_log( GM_LOG_DEBUG, "--------------------------------\n" );
    return;
}


/* free options structure */
void mod_gm_free_opt(mod_gm_opt_t *opt) {
    int i,j;
    if(opt == NULL)
        return;
    for(i=0;i<opt->server_num;i++) {
        free(opt->server_list[i]->host);
        free(opt->server_list[i]);
    }
    for(i=0;i<opt->dupserver_num;i++) {
        free(opt->dupserver_list[i]->host);
        free(opt->dupserver_list[i]);
    }
    for(i=0;i<opt->hostgroups_num;i++)
        free(opt->hostgroups_list[i]);
    for(i=0;i<opt->servicegroups_num;i++)
        free(opt->servicegroups_list[i]);
    for(i=0;i<opt->local_hostgroups_num;i++)
        free(opt->local_hostgroups_list[i]);
    for(i=0;i<opt->local_servicegroups_num;i++)
        free(opt->local_servicegroups_list[i]);
    for(i=0;i<GM_NEBTYPESSIZE;i++) {
        for(j=0;j<opt->exports[i]->elem_number;j++) {
          free(opt->exports[i]->name[j]);
        }
        free(opt->exports[i]);
    }
    for(i=0;i<opt->restrict_path_num;i++) {
        free(opt->restrict_path[i]);
    }
    free(opt->restrict_command_characters);
    free(opt->crypt_key);
    free(opt->keyfile);
    free(opt->message);
    free(opt->delimiter);
    free(opt->pidfile);
    free(opt->logfile);
    free(opt->host);
    free(opt->service);
    free(opt->identifier);
    free(opt->queue_cust_var);
#ifdef EMBEDDEDPERL
    free(opt->p1_file);
#endif
    free(opt);
    opt=NULL;
    return;
}


/* read keyfile */
int read_keyfile(mod_gm_opt_t *opt) {
    FILE *fp;

    if(opt->keyfile == NULL)
        return(GM_ERROR);


    fp = fopen(opt->keyfile,"rb");
    if(fp == NULL) {
        perror(opt->keyfile);
        return(GM_ERROR);
    }
    if(opt->crypt_key != NULL)
        free(opt->crypt_key);
    opt->crypt_key = gm_malloc(GM_BUFFERSIZE);

    if(!fgets(opt->crypt_key, 33, fp)) {
        fclose(fp);
        return(GM_ERROR);
    }
    fclose(fp);
    rtrim(opt->crypt_key);

    return(GM_OK);
}

/* convert to time */
void string2timeval(char * value, struct timeval *t) {
    long double val;
    t->tv_sec  = 0;
    t->tv_usec = 0;
    if(value == NULL) {
        return;
    }
    val = atof(value);
    double2timeval(val, t);
}

/* convert to time */
void double2timeval(long double value, struct timeval *t) {
    t->tv_sec  = (int)value;
    t->tv_usec = (int)((value - (long double)t->tv_sec) * 1000000);
}

/* convert a timeval to double */
long double timeval2double(struct timeval * t) {
    long double val = 0.0;
    if(t != NULL) {
        val = (long double)t->tv_sec + ((long double)t->tv_usec / 1000000);
    }
    return val;
}


/* compare 2 timestructs */
long mod_gm_time_compare(struct timeval * tv1, struct timeval * tv2) {
   long secdiff = (long)(tv1->tv_sec - tv2->tv_sec);
   long usecdiff = (long)(tv1->tv_usec - tv2->tv_usec);
   return secdiff? secdiff: usecdiff;
}


/* set empty default job */
int set_default_job(gm_job_t *job, mod_gm_opt_t *opt) {

    job->type                = NULL;
    job->host_name           = NULL;
    job->service_description = NULL;
    job->result_queue        = NULL;
    job->command_line        = NULL;
    job->source              = NULL;
    job->output              = NULL;
    job->long_output         = NULL;
    job->error               = NULL;
    job->exited_ok           = TRUE;
    job->scheduled_check     = TRUE;
    job->reschedule_check    = TRUE;
    job->return_code         = STATE_OK;
    job->latency             = 0.0;
    job->timeout             = opt->job_timeout;
    job->start_time.tv_sec   = 0L;
    job->start_time.tv_usec  = 0L;
    job->has_been_sent       = FALSE;

    return(GM_OK);
}


/* free the job structure */
int free_job(gm_job_t *job) {

    free(job->type);
    free(job->host_name);
    free(job->service_description);
    free(job->result_queue);
    free(job->command_line);
    if(job->output != NULL)
        free(job->output);
    if(job->long_output != NULL)
        free(job->long_output);
    if(job->source != NULL)
        free(job->source);
    if(job->error != NULL)
        free(job->error);
    free(job);

    return(GM_OK);
}

/* verify if a pid is alive */
int pid_alive(int pid) {
    if(pid < 0) { pid = -pid; }

    /* 1/-1 are undefined pids in our case */
    if(pid == 1)
        return FALSE;

    /* send kill 0 to verify the process still exists */
    if(kill(pid, 0) == 0) {
        return TRUE;
    }

    return FALSE;
}



/* escapes newlines in a string */
char *escapestring(char *rawbuf) {
    char *newbuf=NULL;
    char buff[64];
    register int x,y;

    if(rawbuf==NULL)
        return NULL;

    /* allocate enough memory to escape all chars if necessary */
    if((newbuf=gm_malloc((strlen(rawbuf)*2)+1))==NULL)
        return NULL;

    for(x=0,y=0;rawbuf[x]!=(char)'\x0';x++){

        if(escaped(rawbuf[x])) {
            escape(buff, rawbuf[x]);
            newbuf[y++]=buff[0];
            if(buff[1] != 0)
                newbuf[y++]=buff[1];
        }

        else
            newbuf[y++]=rawbuf[x];
    }
    newbuf[y]='\x0';

    return newbuf;
}

/*
is a character escaped?
Params: ch - character to test
Returns: 1 if escaped, 0 if normal
*/
int escaped(int ch) {
    return strchr("\\\a\b\n\r\t\"\f\v", ch) ? 1 : 0;
}

/*
get the escape sequence for a character
Params: out - output buffer (currently max 2 + nul but allow for more)
ch - the character to escape
*/
void escape(char *out, int ch) {
    switch(ch) {
        case '\n':
            strcpy(out, "\\n"); break;
        case '\t':
            strcpy(out, "\\t"); break;
        case '\v':
            strcpy(out, "\\v"); break;
        case '\b':
            strcpy(out, "\\b"); break;
        case '\r':
            strcpy(out, "\\r"); break;
        case '\f':
            strcpy(out, "\\f"); break;
        case '\a':
            strcpy(out, "\\a"); break;
        case '\\':
            strcpy(out, "\\\\"); break;
        case '\"':
            strcpy(out, "\\\""); break;
        default:
            out[0] = (char) ch;
            out[1] = 0;
            break;
    }
}


char *replace_str(const char *str, const char *old, const char *new) {
    char *ret, *r;
    const char *p, *q;
    size_t oldlen = strlen(old);
    size_t count, retlen, newlen = strlen(new);

    if (str == NULL)
        return NULL;

    if (oldlen != newlen) {
        for (count = 0, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen)
            count++;
        /* this is undefined if p - str > PTRDIFF_MAX */
        retlen = p - str + strlen(p) + count * (newlen - oldlen);
    } else
        retlen = strlen(str);

    if ((ret = gm_malloc(retlen + 1)) == NULL)
        return NULL;

    for (r = ret, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen) {
        /* this is undefined if q - p > PTRDIFF_MAX */
        ptrdiff_t l = q - p;
        memcpy(r, p, l);
        r += l;
        memcpy(r, new, newlen);
        r += newlen;
    }
    strcpy(r, p);

    return ret;
}

/* generic logger function */
void gm_log( int lvl, const char *text, ... ) {
    FILE * fp       = NULL;
    int debug_level = GM_LOG_ERROR;
    int logmode     = GM_LOG_MODE_STDOUT;
    int slevel;
    char * level;
    char buffer1[GM_BUFFERSIZE];
    char buffer2[GM_BUFFERSIZE];
    char buffer3[GM_BUFFERSIZE];
    time_t t;
    va_list ap;
    struct tm now;

    if(mod_gm_opt != NULL) {
        debug_level = mod_gm_opt->debug_level;
        logmode     = mod_gm_opt->logmode;
        fp          = mod_gm_opt->logfile_fp;
    }

    if(logmode == GM_LOG_MODE_CORE) {
        if ( debug_level < 0 )
            return;
        if ( lvl != GM_LOG_ERROR && lvl > debug_level )
            return;

        if ( lvl == GM_LOG_ERROR ) {
            snprintf( buffer1, 22, "mod_gearman: ERROR - " );
        } else {
            snprintf( buffer1, 14, "mod_gearman: " );
        }
        va_start( ap, text );
        vsnprintf( buffer1 + strlen( buffer1 ), sizeof( buffer1 ) - strlen( buffer1 ), text, ap );
        va_end( ap );

        if ( debug_level >= GM_LOG_STDOUT ) {
            printf( "%s", buffer1 );
            return;
        }
        write_core_log( buffer1 );
        return;
    }

    /* check log level */
    if ( lvl != GM_LOG_ERROR && lvl > debug_level ) {
        return;
    }
    if ( lvl == GM_LOG_ERROR ) {
        level  = "ERROR";
        slevel = LOG_ERR;
    }
    else if ( lvl == GM_LOG_INFO ) {
        level  = "INFO ";
        slevel = LOG_INFO;
    }
    else if ( lvl == GM_LOG_DEBUG ) {
        level  = "DEBUG";
        slevel = LOG_DEBUG;
    }
    else if ( lvl >= GM_LOG_TRACE ) {
        level  = "TRACE";
        slevel = LOG_DEBUG;
    }
    else {
        level = "UNKNOWN";
        slevel = LOG_DEBUG;
    }

    /* set timestring */
    t = time(NULL);
    localtime_r(&t, &now);
    strftime(buffer1, sizeof(buffer1), "[%Y-%m-%d %H:%M:%S]", &now );

    /* set timestring */
    snprintf(buffer2, sizeof(buffer2), "[%i][%s]", getpid(), level );

    va_start( ap, text );
    vsnprintf( buffer3, GM_BUFFERSIZE, text, ap );
    va_end( ap );

    if ( debug_level >= GM_LOG_STDOUT || logmode == GM_LOG_MODE_TOOLS ) {
        printf( "%s", buffer3 );
        return;
    }

    if(logmode == GM_LOG_MODE_FILE && fp != NULL) {
        fprintf( fp, "%s%s %s", buffer1, buffer2, buffer3 );
        fflush( fp );
    }
    else if(logmode == GM_LOG_MODE_SYSLOG) {
        syslog(slevel , "%s %s", buffer2, buffer3 );
    }
    else {
        /* stdout logging */
        printf( "%s%s %s", buffer1, buffer2, buffer3 );
    }

    return;
}

/* check server for duplicates */
int check_param_server(gm_server_t * new_server, gm_server_t * server_list[GM_LISTSIZE], int server_num) {
    int i;
    for(i=0;i<server_num;i++) {
        if ( ! strcmp( new_server->host, server_list[i]->host ) && new_server->port == server_list[i]->port ) {
            gm_log( GM_LOG_ERROR, "duplicate definition of server: %s:%i\n", new_server->host, new_server->port);
            return GM_ERROR;
        }
    }
    return GM_OK;
}


/* send results back */
void send_result_back(gm_job_t * exec_job) {
    char * temp_buffer1;
    char * temp_buffer2;
    int result_size;
    gm_log( GM_LOG_TRACE, "send_result_back()\n" );

    /* avoid duplicate returned results */
    if(exec_job->has_been_sent == TRUE) {
        return;
    }
    exec_job->has_been_sent = TRUE;

    if(exec_job->result_queue == NULL) {
        return;
    }
    if(exec_job->output == NULL) {
        return;
    }

    /* workaround for rc 25 bug
     * duplicate jobs from gearmand result in exit code 25 of plugins
     * because they are executed twice and get killed because of using
     * the same ressource.
     * Sending results (when exit code is 25 ) will be skipped with this
     * enabled.
     */
    if( exec_job->return_code == 25 && mod_gm_opt->workaround_rc_25 == GM_ENABLED ) {
        return;
    }

    result_size  = strlen(exec_job->output)+GM_BUFFERSIZE;
    temp_buffer1 = gm_malloc(sizeof(char*)*result_size);
    temp_buffer2 = gm_malloc(sizeof(char*)*result_size);

    gm_log( GM_LOG_TRACE, "queue: %s\n", exec_job->result_queue );
    temp_buffer1[0]='\x0';
    snprintf( temp_buffer1, result_size-1, "host_name=%s\ncore_start_time=%Lf\nstart_time=%Lf\nfinish_time=%Lf\nreturn_code=%i\nexited_ok=%i\nsource=%s\n",
              exec_job->host_name,
              timeval2double(&exec_job->next_check),
              timeval2double(&exec_job->start_time),
              timeval2double(&exec_job->finish_time),
              exec_job->return_code,
              exec_job->exited_ok,
              exec_job->source
            );
    temp_buffer1[result_size-1]='\x0';

    if(exec_job->service_description != NULL) {
        temp_buffer2[0]='\x0';
        strcat(temp_buffer2, "service_description=");
        strcat(temp_buffer2, exec_job->service_description);
        strcat(temp_buffer2, "\n");

        strcat(temp_buffer1, temp_buffer2);
    }
    temp_buffer1[result_size]='\x0';

    if(exec_job->output != NULL) {
        temp_buffer2[0]='\x0';
        strcat(temp_buffer2, "output=");
        if(mod_gm_opt->debug_result) {
            strcat(temp_buffer2, "(");
            strcat(temp_buffer2, hostname);
            strcat(temp_buffer2, ") - ");
        }
        strcat(temp_buffer2, exec_job->output);
        if(mod_gm_opt->show_error_output && exec_job->error != NULL && strlen(exec_job->error) > 0) {
            if(strlen(exec_job->output) > 0)
                strcat(temp_buffer2, "\\n");
            strcat(temp_buffer2, "[");
            strcat(temp_buffer2, exec_job->error);
            strcat(temp_buffer2, "] ");
        }
        strcat(temp_buffer2, "\n\n\n");
        strcat(temp_buffer1, temp_buffer2);
    }
    strcat(temp_buffer1, "\n");
    temp_buffer1[result_size]='\x0';

    gm_log( GM_LOG_TRACE, "data:\n%s\n", temp_buffer1);

    if(add_job_to_queue( current_client,
                         mod_gm_opt->server_list,
                         exec_job->result_queue,
                         NULL,
                         temp_buffer1,
                         GM_JOB_PRIO_NORMAL,
                         GM_DEFAULT_JOB_RETRIES,
                         mod_gm_opt->transportmode
                        ) == GM_OK) {
        gm_log( GM_LOG_TRACE, "send_result_back() finished successfully\n" );
    }
    else {
        gm_log( GM_LOG_TRACE, "send_result_back() finished unsuccessfully\n" );
    }

    if( mod_gm_opt->dupserver_num ) {
        temp_buffer2[0]='\x0';
        if(mod_gm_opt->dup_results_are_passive) {
            strcat(temp_buffer2, "type=passive\n");
        }
        strcat(temp_buffer2, temp_buffer1);
        temp_buffer2[result_size]='\x0';
        if( add_job_to_queue( current_client_dup,
                              mod_gm_opt->dupserver_list,
                              exec_job->result_queue,
                              NULL,
                              temp_buffer2,
                              GM_JOB_PRIO_NORMAL,
                              GM_DEFAULT_JOB_RETRIES,
                              mod_gm_opt->transportmode
                            ) == GM_OK) {
            gm_log( GM_LOG_TRACE, "send_result_back() finished successfully for duplicate server.\n" );
        }
        else {
            gm_log( GM_LOG_TRACE, "send_result_back() finished unsuccessfully for duplicate server\n" );
        }
    }
    else {
        gm_log( GM_LOG_TRACE, "send_result_back() has no duplicate servers to send to.\n" );
    }
    free(temp_buffer1);
    free(temp_buffer2);
    return;
}


/* create md5 sum for char[] */
char *md5sum(char *text) {
    unsigned char sum[16];
    char *result=NULL;

    /* allocate enough memory to escape all chars if necessary */
    if((result=gm_malloc(33))==NULL)
        return NULL;

    md5((unsigned char *)text, strlen(text), sum);
    snprintf(result, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
           sum[0],sum[1],sum[2],sum[3],sum[4],sum[5],sum[6],sum[7],sum[8],sum[9],sum[10],sum[11],sum[12],sum[13],sum[14],sum[15]);

    return result;
}

/* add parsed server to list */
void add_server(int * server_num, gm_server_t * server_list[GM_LISTSIZE], char * servername) {
    gm_server_t *new_server;
    char * server   = gm_strdup( servername );
    char * server_c = server;
    char * host     = strsep( &server, ":" );
    char * port_val = strsep( &server, "\x0" );
    in_port_t port  = GM_SERVER_DEFAULT_PORT;
    if(port_val != NULL) {
        port  = ( in_port_t ) atoi( port_val );
    }
    new_server = gm_malloc(sizeof(gm_server_t));
    if(!strcmp(host, "")) {
        new_server->host = gm_strdup("0.0.0.0");
    } else {
        new_server->host = gm_strdup(host);
    }
    new_server->port = port;
    if(check_param_server(new_server, server_list, *server_num) == GM_OK) {
        server_list[*server_num] = new_server;
        *server_num = *server_num + 1;
    } else {
        free(new_server->host);
        free(new_server);
    }
    free(server_c);
    return;
}

/* check if string starts with another string */
int starts_with(const char *pre, const char *str) {
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? FALSE : strncmp(pre, str, lenpre) == 0;
}


/* read from filepointer as long as it has data and return size of string */
int read_filepointer(char **target, FILE* input) {
    char buffer[GM_BUFFERSIZE] = "";
    int bytes, size, total;
    strcpy(buffer,"");
    size  = GM_BUFFERSIZE;
    total = size;
    while(fgets(buffer,sizeof(buffer)-1, input)){
        if(size >= GM_MAX_OUTPUT) {
            gm_log( GM_LOG_INFO, "plugin output exceeds %d bytes, cutting off\n", GM_MAX_OUTPUT );
            while(fgets(buffer,sizeof(buffer)-1, input)){}
            continue;
        }
        bytes = strlen(buffer);
        if(total < bytes + size) {
            *target = gm_realloc(*target, total+GM_BUFFERSIZE);
            total += GM_BUFFERSIZE;
        }
        size += bytes;
        strncat(*target, buffer, bytes);
    }
    return(size);
}

/* read from pipe as long as it has data and return size of string */
int read_pipe(char **target, int input) {
    char buffer[GM_BUFFERSIZE] = "";
    int bytes, size, total;
    strcpy(buffer,"");
    size  = GM_BUFFERSIZE;
    total = size;
    while(read(input, buffer, sizeof(buffer)-1)){
        if(size >= GM_MAX_OUTPUT) {
            gm_log( GM_LOG_INFO, "plugin output exceeds %d bytes, cutting off\n", GM_MAX_OUTPUT );
            while(read(input, buffer, sizeof(buffer)-1)){}
            continue;
        }
        bytes = strlen(buffer);
        if(total < bytes + size) {
            *target = gm_realloc(*target, total+GM_BUFFERSIZE);
            total += GM_BUFFERSIZE;
        }
        size += bytes;
        strncat(*target, buffer, bytes);
    }
    return(size);
}
