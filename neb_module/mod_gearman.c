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
#include "result_thread.h"
#include "mod_gearman.h"
#include "gearman_utils.h"

/* specify event broker API version (required) */
NEB_API_VERSION( CURRENT_NEB_API_VERSION )

/* import some global variables */
#ifdef USENAGIOS3
extern int            event_broker_options;
#endif
#if defined(USENAEMON) || defined(USENAGIOS4)
extern unsigned long  event_broker_options;
#ifdef USENAEMON
#define my_free nm_free
#endif
#endif
extern int            currently_running_host_checks;
extern int            currently_running_service_checks;
extern int            service_check_timeout;
extern int            host_check_timeout;
#ifdef USENAGIOS
extern timed_event  * event_list_low;
extern timed_event  * event_list_low_tail;
#endif
extern int            process_performance_data;
#ifdef USENAGIOS3
extern check_result   check_result_info;
extern check_result * check_result_list;
#endif

/* global variables */
#ifdef USENAGIOS3
static check_result * mod_gm_result_list = 0;
#endif
#if defined(USENAEMON) || defined(USENAGIOS4)
static objectlist * mod_gm_result_list = 0;
#endif
static pthread_mutex_t mod_gm_result_list_mutex = PTHREAD_MUTEX_INITIALIZER;
void *gearman_module_handle=NULL;
gearman_client_st client;

int send_now, result_threads_running;
pthread_t result_thr[GM_LISTSIZE];
char target_queue[GM_BUFFERSIZE];
char temp_buffer[GM_BUFFERSIZE];
char uniq[GM_BUFFERSIZE];

static void  register_neb_callbacks(void);
static int   read_arguments( const char * );
static int   verify_options(mod_gm_opt_t *opt);
static int   handle_host_check( int,void * );
static int   handle_svc_check( int,void * );
static int   handle_eventhandler( int,void * );
static int   handle_perfdata(int e, void *);
static int   handle_export(int e, void *);
static void  set_target_queue( host *, service * );
static int   handle_process_events( int, void * );
#ifdef USENAGIOS
static int   handle_timed_events( int, void * );
#endif
static void  start_threads(void);
#ifdef USENAGIOS3
static check_result * merge_result_lists(check_result * lista, check_result * listb);
static void move_results_to_core_3x(void);
#endif
#ifdef USENAGIOS4
static void move_results_to_core(void);
#endif
#ifdef USENAEMON
static void move_results_to_core(struct nm_event_execution_properties *evprop);
#endif

int nebmodule_init( int flags, char *args, nebmodule *handle ) {
    int i;
    int broker_option_errors = 0;
    send_now                 = FALSE;
    result_threads_running   = 0;

    /* save our handle */
    gearman_module_handle=handle;

    /* set some module info */
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_TITLE,   "Mod-Gearman" );
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_AUTHOR,  "Sven Nierlein" );
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_TITLE,   "Copyright (c) 2010-2011 Sven Nierlein" );
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_VERSION, GM_VERSION );
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_LICENSE, "GPL v3" );
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_DESC,    "distribute host/service checks and eventhandler via gearman" );

    mod_gm_opt = gm_malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);

    /* parse arguments */
    gm_log( GM_LOG_DEBUG, "Version %s\n", GM_VERSION );
    gm_log( GM_LOG_DEBUG, "args: %s\n", args );
    gm_log( GM_LOG_TRACE, "nebmodule_init(%i, %i)\n", flags );
    gm_log( GM_LOG_DEBUG, "running on libgearman %s\n", gearman_version() );

    if( read_arguments( args ) == GM_ERROR )
        return NEB_ERROR;

    /* check for minimum eventbroker options */
    if(!(event_broker_options & BROKER_PROGRAM_STATE)) {
        gm_log( GM_LOG_ERROR, "mod_gearman needs BROKER_PROGRAM_STATE (%i) event_broker_options enabled to work\n", BROKER_PROGRAM_STATE );
        broker_option_errors++;
    }
    if(!(event_broker_options & BROKER_TIMED_EVENTS)) {
        gm_log( GM_LOG_ERROR, "mod_gearman needs BROKER_TIMED_EVENTS (%i) event_broker_options enabled to work\n", BROKER_TIMED_EVENTS );
        broker_option_errors++;
    }
    if(    (    mod_gm_opt->perfdata != GM_DISABLED
             || mod_gm_opt->hostgroups_num > 0
             || mod_gm_opt->hosts == GM_ENABLED
           )
        && !(event_broker_options & BROKER_HOST_CHECKS)) {
        gm_log( GM_LOG_ERROR, "mod_gearman needs BROKER_HOST_CHECKS (%i) event_broker_options enabled to work\n", BROKER_HOST_CHECKS );
        broker_option_errors++;
    }
    if(    (    mod_gm_opt->perfdata != GM_DISABLED
             || mod_gm_opt->servicegroups_num > 0
             || mod_gm_opt->services == GM_ENABLED
           )
        && !(event_broker_options & BROKER_SERVICE_CHECKS)) {
        gm_log( GM_LOG_ERROR, "mod_gearman needs BROKER_SERVICE_CHECKS (%i) event_broker_options enabled to work\n", BROKER_SERVICE_CHECKS );
        broker_option_errors++;
    }
    if(mod_gm_opt->events == GM_ENABLED && !(event_broker_options & BROKER_EVENT_HANDLERS)) {
        gm_log( GM_LOG_ERROR, "mod_gearman needs BROKER_EVENT_HANDLERS (%i) event_broker option enabled to work\n", BROKER_EVENT_HANDLERS );
        broker_option_errors++;
    }
    if(broker_option_errors > 0)
        return NEB_ERROR;

    /* check the minimal gearman version */
    if((float)atof(gearman_version()) < (float)GM_MIN_LIB_GEARMAN_VERSION) {
        gm_log( GM_LOG_ERROR, "minimum version of libgearman is %.2f, yours is %.2f\n", (float)GM_MIN_LIB_GEARMAN_VERSION, (float)atof(gearman_version()) );
        return NEB_ERROR;
    }

    /* init crypto functions */
    if(mod_gm_opt->encryption == GM_ENABLED) {
        if(mod_gm_opt->crypt_key == NULL) {
            gm_log( GM_LOG_ERROR, "no encryption key provided, please use --key=... or keyfile=...\n");
            return NEB_ERROR;
        }
        mod_gm_crypt_init(mod_gm_opt->crypt_key);
    } else {
        mod_gm_opt->transportmode = GM_ENCODE_ONLY;
    }

    /* create client */
    if ( create_client( mod_gm_opt->server_list, &client ) != GM_OK ) {
        gm_log( GM_LOG_ERROR, "cannot start client\n" );
        return NEB_ERROR;
    }

    /* register callback for process event where everything else starts */
    neb_register_callback( NEBCALLBACK_PROCESS_DATA, gearman_module_handle, 0, handle_process_events );
#ifdef USENAGIOS
    neb_register_callback( NEBCALLBACK_TIMED_EVENT_DATA, gearman_module_handle, 0, handle_timed_events );
#endif
#ifdef USENAEMON
    schedule_event(1, move_results_to_core, NULL);
#endif

    /* register export callbacks */
    for(i=0;i<GM_NEBTYPESSIZE;i++) {
        if(mod_gm_opt->exports[i]->elem_number > 0)
            neb_register_callback( i, gearman_module_handle, 0, handle_export );
    }

    /* log at least one line into the core logfile */
    if ( mod_gm_opt->logmode != GM_LOG_MODE_CORE ) {
        int logmode_saved = mod_gm_opt->logmode;
        mod_gm_opt->logmode = GM_LOG_MODE_CORE;
        gm_log( GM_LOG_INFO,  "initialized version %s (libgearman %s)\n", GM_VERSION, gearman_version() );
        mod_gm_opt->logmode = logmode_saved;
    }

    gm_log( GM_LOG_DEBUG, "finished initializing\n" );

    return NEB_OK;
}


/* register eventhandler callback */
static void register_neb_callbacks(void) {

    /* only if we have hostgroups defined or general hosts enabled */
    if ( mod_gm_opt->do_hostchecks == GM_ENABLED && ( mod_gm_opt->hostgroups_num > 0 || mod_gm_opt->hosts == GM_ENABLED || mod_gm_opt->queue_cust_var ))
        neb_register_callback( NEBCALLBACK_HOST_CHECK_DATA,    gearman_module_handle, 0, handle_host_check );

    /* only if we have groups defined or general services enabled */
    if ( mod_gm_opt->servicegroups_num > 0 || mod_gm_opt->hostgroups_num > 0 || mod_gm_opt->services == GM_ENABLED || mod_gm_opt->queue_cust_var )
        neb_register_callback( NEBCALLBACK_SERVICE_CHECK_DATA, gearman_module_handle, 0, handle_svc_check );

    if ( mod_gm_opt->events == GM_ENABLED )
        neb_register_callback( NEBCALLBACK_EVENT_HANDLER_DATA, gearman_module_handle, 0, handle_eventhandler );

    if ( mod_gm_opt->perfdata != GM_DISABLED ) {
        if(process_performance_data == 0)
            gm_log( GM_LOG_INFO, "Warning: process_performance_data is disabled globally, cannot process performance data\n" );
        neb_register_callback( NEBCALLBACK_HOST_CHECK_DATA, gearman_module_handle, 0, handle_perfdata );
        neb_register_callback( NEBCALLBACK_SERVICE_CHECK_DATA, gearman_module_handle, 0, handle_perfdata );
    }

    gm_log( GM_LOG_DEBUG, "registered neb callbacks\n" );
}


/* deregister all events */
int nebmodule_deinit( int flags, int reason ) {
    int x;

    gm_log( GM_LOG_TRACE, "nebmodule_deinit(%i, %i)\n", flags, reason );

    /* should be removed already, but just for the case it wasn't */
    neb_deregister_callback( NEBCALLBACK_PROCESS_DATA, gearman_module_handle );
    neb_deregister_callback( NEBCALLBACK_TIMED_EVENT_DATA, gearman_module_handle );

    /* only if we have hostgroups defined or general hosts enabled */
    if ( mod_gm_opt->do_hostchecks == GM_ENABLED && ( mod_gm_opt->hostgroups_num > 0 || mod_gm_opt->hosts == GM_ENABLED ))
        neb_deregister_callback( NEBCALLBACK_HOST_CHECK_DATA, gearman_module_handle );

    /* only if we have groups defined or general services enabled */
    if ( mod_gm_opt->servicegroups_num > 0 || mod_gm_opt->hostgroups_num > 0 || mod_gm_opt->services == GM_ENABLED )
        neb_deregister_callback( NEBCALLBACK_SERVICE_CHECK_DATA, gearman_module_handle );

    if ( mod_gm_opt->events == GM_ENABLED )
        neb_deregister_callback( NEBCALLBACK_EVENT_HANDLER_DATA, gearman_module_handle );

    if ( mod_gm_opt->perfdata != GM_DISABLED ) {
        neb_deregister_callback( NEBCALLBACK_HOST_CHECK_DATA, gearman_module_handle );
        neb_deregister_callback( NEBCALLBACK_SERVICE_CHECK_DATA, gearman_module_handle );
    }

    /* register export callbacks */
    for(x=0;x<GM_NEBTYPESSIZE;x++) {
        if(mod_gm_opt->exports[x]->elem_number > 0)
            neb_deregister_callback( x, gearman_module_handle );
    }

    neb_deregister_callback( NEBCALLBACK_PROCESS_DATA, gearman_module_handle );

    gm_log( GM_LOG_DEBUG, "deregistered callbacks\n" );

    /* stop result threads */
    for(x = 0; x < result_threads_running; x++) {
        pthread_cancel(result_thr[x]);
        pthread_join(result_thr[x], NULL);
    }

    /* cleanup */
    free_client(&client);

    /* close old logfile */
    if(mod_gm_opt->logfile_fp != NULL) {
        fclose(mod_gm_opt->logfile_fp);
    }

    mod_gm_free_opt(mod_gm_opt);

    return NEB_OK;
}


/* handle timed events */
#ifdef USENAGIOS
static int handle_timed_events( int event_type, void *data ) {
    nebstruct_timed_event_data * ted = (nebstruct_timed_event_data *)data;

    /* sanity checks */
    if (event_type != NEBCALLBACK_TIMED_EVENT_DATA || ted == 0)
        return NEB_ERROR;

    /* we only care about REAPER events */
    if (ted->event_type != EVENT_CHECK_REAPER)
        return NEB_OK;

    gm_log( GM_LOG_TRACE, "handle_timed_events(%i, data)\n", event_type, ted->event_type );

#ifdef USENAGIOS3
    move_results_to_core_3x();
#endif
#ifdef USENAGIOS4
    move_results_to_core();
#endif

    return NEB_OK;
}
#endif

#ifdef USENAGIOS3
/* merge results with core */
static check_result * merge_result_lists(check_result * lista, check_result * listb) {
    check_result * result = 0;

    check_result ** iter;
    for (iter = &result; lista && listb; iter = &(*iter)->next) {
        if (mod_gm_time_compare(&lista->finish_time, &listb->finish_time) <= 0) {
            *iter = lista; lista = lista->next;
        } else {
            *iter = listb; listb = listb->next;
        }
    }

    *iter = lista? lista: listb;

    return result;
}
#endif

/* insert results list into naemon/nagios4 core */
#if defined(USENAEMON) || defined(USENAGIOS4)
#ifdef USENAEMON
static void move_results_to_core(struct nm_event_execution_properties *evprop) {
#endif
#ifdef USENAGIOS4
static void move_results_to_core() {
#endif
#ifdef USENAEMON
    if(evprop->execution_type == EVENT_EXEC_NORMAL) {
#endif
    /* safely save off currently local list */
    pthread_mutex_lock(&mod_gm_result_list_mutex);

    objectlist *tmp_list = NULL;
    for( ; mod_gm_result_list; mod_gm_result_list = mod_gm_result_list->next) {
        free(tmp_list);
        process_check_result(mod_gm_result_list->object_ptr);
        free_check_result(mod_gm_result_list->object_ptr);
        check_result *info = mod_gm_result_list->object_ptr;
        my_free(info->source);
        free(mod_gm_result_list->object_ptr);
        tmp_list = mod_gm_result_list;
    }
    mod_gm_result_list = 0;
    free(tmp_list);

    pthread_mutex_unlock(&mod_gm_result_list_mutex);
#ifdef USENAEMON
        schedule_event(1, move_results_to_core, NULL);
    }
#endif
}
#endif

/* insert results list into nagios 3 core */
#ifdef USENAGIOS3
static void move_results_to_core_3x() {
   check_result * local;

   /* safely save off currently local list */
   pthread_mutex_lock(&mod_gm_result_list_mutex);
   local = mod_gm_result_list;
   mod_gm_result_list = 0;
   pthread_mutex_unlock(&mod_gm_result_list_mutex);

   /* merge local into check_result_list, store in check_result_list */
   check_result_list = merge_result_lists(local, check_result_list);
}
#endif

/* add list to gearman result list */
#if defined(USENAEMON) || defined(USENAGIOS4)
void mod_gm_add_result_to_list(check_result * newcr) {
    pthread_mutex_lock(&mod_gm_result_list_mutex);
    add_object_to_objectlist(&mod_gm_result_list, newcr);
    pthread_mutex_unlock(&mod_gm_result_list_mutex);
}
#endif

/* add list to gearman result list */
#ifdef USENAGIOS3
void mod_gm_add_result_to_list(check_result * newcr) {
   check_result ** curp;

   assert(newcr);

   pthread_mutex_lock(&mod_gm_result_list_mutex);

   for (curp = &mod_gm_result_list; *curp; curp = &(*curp)->next)
      if (mod_gm_time_compare(&(*curp)->finish_time, &newcr->finish_time) >= 0)
         break;

   newcr->next = *curp;
   *curp = newcr;

   pthread_mutex_unlock(&mod_gm_result_list_mutex);
}
#endif

/* handle process events */
static int handle_process_events( int event_type, void *data ) {
    int x=0;
    struct nebstruct_process_struct *ps;

    gm_log( GM_LOG_TRACE, "handle_process_events(%i, data)\n", event_type );

    ps = ( struct nebstruct_process_struct * )data;
    if ( ps->type == NEBTYPE_PROCESS_EVENTLOOPSTART ) {

        register_neb_callbacks();
        start_threads();
        send_now = TRUE;

        /* verify names of supplied groups
         * this cannot be done befor naemon has finished reading his config
         * verify local servicegroups names
         */
        while ( mod_gm_opt->local_servicegroups_list[x] != NULL ) {
            servicegroup * temp_servicegroup = find_servicegroup( mod_gm_opt->local_servicegroups_list[x] );
            if( temp_servicegroup == NULL ) {
                gm_log( GM_LOG_INFO, "Warning: servicegroup '%s' does not exist, possible typo?\n", mod_gm_opt->local_servicegroups_list[x] );
            }
            x++;
        }

        /* verify local hostgroup names */
        x = 0;
        while ( mod_gm_opt->local_hostgroups_list[x] != NULL ) {
            hostgroup * temp_hostgroup = find_hostgroup( mod_gm_opt->local_hostgroups_list[x] );
            if( temp_hostgroup == NULL ) {
                gm_log( GM_LOG_INFO, "Warning: hostgroup '%s' does not exist, possible typo?\n", mod_gm_opt->local_hostgroups_list[x] );
            }
            x++;
        }

        /* verify servicegroups names */
        x = 0;
        while ( mod_gm_opt->servicegroups_list[x] != NULL ) {
            servicegroup * temp_servicegroup = find_servicegroup( mod_gm_opt->servicegroups_list[x] );
            if( temp_servicegroup == NULL ) {
                gm_log( GM_LOG_INFO, "Warning: servicegroup '%s' does not exist, possible typo?\n", mod_gm_opt->servicegroups_list[x] );
            }
            x++;
        }

        /* verify hostgroup names */
        x = 0;
        while ( mod_gm_opt->hostgroups_list[x] != NULL ) {
            hostgroup * temp_hostgroup = find_hostgroup( mod_gm_opt->hostgroups_list[x] );
            if( temp_hostgroup == NULL ) {
                gm_log( GM_LOG_INFO, "Warning: hostgroup '%s' does not exist, possible typo?\n", mod_gm_opt->hostgroups_list[x] );
            }
            x++;
        }
    }

    return NEB_OK;
}


/* handle eventhandler events */
static int handle_eventhandler( int event_type, void *data ) {
    nebstruct_event_handler_data * ds;
    host * hst    = NULL;
    service * svc = NULL;
    struct timeval core_time;
    gettimeofday(&core_time,NULL);

    gm_log( GM_LOG_TRACE, "handle_eventhandler(%i, data)\n", event_type );

    if ( event_type != NEBCALLBACK_EVENT_HANDLER_DATA )
        return NEB_OK;

    ds = ( nebstruct_event_handler_data * )data;

    if ( ds->type != NEBTYPE_EVENTHANDLER_START ) {
        gm_log( GM_LOG_TRACE, "skiped type %i, expecting: %i\n", ds->type, NEBTYPE_EVENTHANDLER_START );
        return NEB_OK;
    }

    gm_log( GM_LOG_DEBUG, "got eventhandler event\n" );
    gm_log( GM_LOG_TRACE, "got eventhandler event: %s\n", ds->command_line );

    /* service event handler? */
    if(ds->service_description != NULL) {
        if((svc=ds->object_ptr)==NULL) {
            gm_log( GM_LOG_ERROR, "Eventhandler handler received NULL service object pointer.\n" );
            return NEB_OK;
        }
        if((hst=svc->host_ptr)==NULL) {
            gm_log( GM_LOG_ERROR, "Service handler received NULL host object pointer.\n" );
            return NEB_OK;
        }
    }
    else {
        if((hst=ds->object_ptr)==NULL) {
            gm_log( GM_LOG_ERROR, "Host handler received NULL host object pointer.\n" );
            return NEB_OK;
        }
    }

    /* local eventhandler? */
    set_target_queue( hst, svc );
    if(!strcmp( target_queue, "" )) {
        if(svc != NULL) {
            gm_log( GM_LOG_DEBUG, "passing by local service eventhandler: %s - %s\n", svc->host_name, svc->description );
        } else {
            gm_log( GM_LOG_DEBUG, "passing by local host eventhandler: %s\n", hst->name );
        }
        return NEB_OK;
    }

    if(mod_gm_opt->route_eventhandler_like_checks != GM_ENABLED || !strcmp( target_queue, "host" ) || !strcmp( target_queue, "service" )) {
        target_queue[0] = '\x0';
        snprintf( target_queue, GM_BUFFERSIZE-1, "eventhandler" );
    }

    gm_log( GM_LOG_DEBUG, "eventhandler for queue %s\n", target_queue );

    temp_buffer[0]='\x0';
    snprintf( temp_buffer,GM_BUFFERSIZE-1,
                "type=eventhandler\nstart_time=%i.0\ncore_time=%i.%i\ncommand_line=%s\n\n\n",
                (int)core_time.tv_sec,
                (int)core_time.tv_sec,
                (int)core_time.tv_usec,
                ds->command_line
    );

    if(add_job_to_queue( &client,
                         mod_gm_opt->server_list,
                         target_queue,
                         NULL,
                         temp_buffer,
                         GM_JOB_PRIO_NORMAL,
                         GM_DEFAULT_JOB_RETRIES,
                         mod_gm_opt->transportmode,
                         FALSE
                        ) == GM_OK) {
        gm_log( GM_LOG_TRACE, "handle_eventhandler() finished successfully\n" );
    }
    else {
        gm_log( GM_LOG_TRACE, "handle_eventhandler() finished unsuccessfully\n" );
    }

    /* tell naemon to not execute */
    return NEBERROR_CALLBACKOVERRIDE;
}


/* handle host check events */
static int handle_host_check( int event_type, void *data ) {
    nebstruct_host_check_data * hostdata;
    char *raw_command=NULL;
    char *processed_command=NULL;
    host * hst;
#ifdef USENAGIOS
    check_result * chk_result;
    int check_options;
#endif
    struct timeval core_time;
    struct tm next_check;
    char buffer1[GM_BUFFERSIZE];
#if defined(USENAEMON) || defined(USENAGIOS4)
    nagios_macros mac;
#endif

    gettimeofday(&core_time,NULL);

    gm_log( GM_LOG_TRACE, "handle_host_check(%i)\n", event_type );

    if ( mod_gm_opt->do_hostchecks != GM_ENABLED )
        return NEB_OK;

    hostdata = ( nebstruct_host_check_data * )data;

    gm_log( GM_LOG_TRACE, "---------------\nhost Job -> %i, %i\n", event_type, hostdata->type );

    if ( event_type != NEBCALLBACK_HOST_CHECK_DATA )
        return NEB_OK;

    /* ignore non-initiate host checks */
    if (   hostdata->type != NEBTYPE_HOSTCHECK_ASYNC_PRECHECK
        && hostdata->type != NEBTYPE_HOSTCHECK_SYNC_PRECHECK)
        return NEB_OK;

    /* get objects and set target function */
    if((hst=hostdata->object_ptr)==NULL) {
        gm_log( GM_LOG_ERROR, "Host handler received NULL host object pointer.\n" );
        return NEBERROR_CALLBACKCANCEL;
    }
    set_target_queue( hst, NULL );

    /* local check? */
    if(!strcmp( target_queue, "" )) {
        gm_log( GM_LOG_DEBUG, "passing by local hostcheck: %s\n", hostdata->host_name );
        return NEB_OK;
    }

    gm_log( GM_LOG_DEBUG, "received job for queue %s: %s\n", target_queue, hostdata->host_name );

    /* as we have to intercept host checks so early
     * (we cannot cancel checks otherwise)
     * we have to do some host check logic here
     * taken from checks.c:
     */
#ifdef USENAGIOS
    /* clear check options - we don't want old check options retained */
    check_options = hst->check_options;
    hst->check_options = CHECK_OPTION_NONE;
#endif

    /* unset the freshening flag, otherwise only the first freshness check would be run */
    hst->is_being_freshened=FALSE;

    /* adjust host check attempt */
#ifdef USENAGIOS3
    adjust_host_check_attempt_3x(hst,TRUE);
#endif
#if defined(USENAEMON) || defined(USENAGIOS4)
    adjust_host_check_attempt(hst,TRUE);
#endif

    temp_buffer[0]='\x0';

    /* grab the host macro variables */
#ifdef USENAGIOS3
    clear_volatile_macros();
    grab_host_macros(hst);
#endif
#if defined(USENAEMON) || defined(USENAGIOS4)
    memset(&mac, 0, sizeof(mac));
    clear_volatile_macros_r(&mac);
    grab_host_macros_r(&mac, hst);
#endif

    /* get the raw command line */
#ifdef USENAGIOS3
    get_raw_command_line(hst->check_command_ptr,hst->host_check_command,&raw_command,0);
#endif
#if defined(USENAEMON) || defined(USENAGIOS4)
    get_raw_command_line_r(&mac, hst->check_command_ptr, hst->check_command, &raw_command, 0);
#endif
    if(raw_command==NULL){
        gm_log( GM_LOG_ERROR, "Raw check command for host '%s' was NULL - aborting.\n",hst->name );
        return NEBERROR_CALLBACKCANCEL;
    }

    /* process any macros contained in the argument */
#ifdef USENAGIOS3
    process_macros(raw_command,&processed_command,0);
#endif
#if defined(USENAEMON) || defined(USENAGIOS4)
    process_macros_r(&mac, raw_command, &processed_command, 0);
#endif
    if(processed_command==NULL){
        gm_log( GM_LOG_ERROR, "Processed check command for host '%s' was NULL - aborting.\n",hst->name);
        return NEBERROR_CALLBACKCANCEL;
    }
#if defined(USENAEMON)
    /* naemon sends unescaped newlines from ex.: the LONGPLUGINOUTPUT macro, so we have to escape
     * them ourselves: https://github.com/naemon/naemon-core/issues/153 */
    char *tmp = replace_str(processed_command, "\n", "\\n");
    free(processed_command);
    processed_command = replace_str(tmp, "\n", "\\n");
    free(tmp);
#endif

    /* log latency */
    if(mod_gm_opt->debug_level >= GM_LOG_DEBUG) {
        localtime_r(&hst->next_check, &next_check);
        strftime(buffer1, sizeof(buffer1), "%Y-%m-%d %H:%M:%S", &next_check );
        gm_log( GM_LOG_DEBUG, "host: '%s', next_check is at %s, latency so far: %i\n", hst->name, buffer1, ((int)core_time.tv_sec - (int)hst->next_check));
    }

    /* increment number of host checks that are currently running */
    currently_running_host_checks++;

    /* set the execution flag */
    hst->is_executing=TRUE;

    gm_log( GM_LOG_TRACE, "cmd_line: %s\n", processed_command );

    snprintf( temp_buffer,GM_BUFFERSIZE-1,"type=host\nresult_queue=%s\nhost_name=%s\nstart_time=%i.0\nnext_check=%i.0\ntimeout=%d\ncore_time=%i.%i\ncommand_line=%s\n\n\n",
              mod_gm_opt->result_queue,
              hst->name,
              (int)hst->next_check,
              (int)hst->next_check,
              host_check_timeout,
              (int)core_time.tv_sec,
              (int)core_time.tv_usec,
              processed_command
            );

    if(add_job_to_queue( &client,
                         mod_gm_opt->server_list,
                         target_queue,
                        (mod_gm_opt->use_uniq_jobs == GM_ENABLED ? hst->name : NULL),
                         temp_buffer,
                         GM_JOB_PRIO_NORMAL,
                         GM_DEFAULT_JOB_RETRIES,
                         mod_gm_opt->transportmode,
                         TRUE
                        ) == GM_OK) {
    }
    else {
        my_free(raw_command);
        my_free(processed_command);
#if defined(USENAEMON)
        clear_volatile_macros_r(&mac);
#endif
        /* unset the execution flag */
        hst->is_executing=FALSE;

        /* decrement number of host checks that are currently running */
        currently_running_host_checks--;

        gm_log( GM_LOG_TRACE, "handle_host_check() finished unsuccessfully -> %d\n", NEBERROR_CALLBACKCANCEL );
        return NEBERROR_CALLBACKCANCEL;
    }

    /* clean up */
    my_free(raw_command);
    my_free(processed_command);

    /* orphaned check - submit fake result to mark host as orphaned */
#ifdef USENAGIOS
    if(mod_gm_opt->orphan_host_checks == GM_ENABLED && check_options & CHECK_OPTION_ORPHAN_CHECK) {
        gm_log( GM_LOG_DEBUG, "host check for %s orphaned\n", hst->name );
        if ( ( chk_result = ( check_result * )gm_malloc( sizeof *chk_result ) ) == 0 )
            return NEBERROR_CALLBACKCANCEL;
        snprintf( temp_buffer,GM_BUFFERSIZE-1,"(host check orphaned, is the mod-gearman worker on queue '%s' running?)\n", target_queue);
        init_check_result(chk_result);
        chk_result->host_name           = gm_strdup( hst->name );
        chk_result->scheduled_check     = TRUE;
        chk_result->reschedule_check    = TRUE;
        chk_result->output_file         = 0;
        chk_result->output_file_fp      = NULL;
        chk_result->output              = gm_strdup(temp_buffer);
        chk_result->return_code         = mod_gm_opt->orphan_return;
        chk_result->check_options       = CHECK_OPTION_NONE;
        chk_result->object_check_type   = HOST_CHECK;
        chk_result->check_type          = HOST_CHECK_ACTIVE;
        chk_result->start_time.tv_sec   = (unsigned long)time(NULL);
        chk_result->finish_time.tv_sec  = (unsigned long)time(NULL);
        chk_result->latency             = 0;
        mod_gm_add_result_to_list( chk_result );
        chk_result = NULL;
    }
#endif

    /* tell naemon to not execute */
    gm_log( GM_LOG_TRACE, "handle_host_check() finished successfully -> %d\n", NEBERROR_CALLBACKOVERRIDE );
    return NEBERROR_CALLBACKOVERRIDE;
}


/* handle service check events */
static int handle_svc_check( int event_type, void *data ) {
    host * hst   = NULL;
    service * svc = NULL;
    char *raw_command=NULL;
    char *processed_command=NULL;
    nebstruct_service_check_data * svcdata;
    int prio = GM_JOB_PRIO_LOW;
#ifdef USENAGIOS
    check_result * chk_result;
#endif
    struct timeval core_time;
    struct tm next_check;
    char buffer1[GM_BUFFERSIZE];
#if defined(USENAEMON) || defined(USENAGIOS4)
    nagios_macros mac;
#endif

    gettimeofday(&core_time,NULL);

    gm_log( GM_LOG_TRACE, "handle_svc_check(%i, data)\n", event_type );
    svcdata = ( nebstruct_service_check_data * )data;

    if ( event_type != NEBCALLBACK_SERVICE_CHECK_DATA )
        return NEB_OK;

    /* ignore non-initiate service checks */
    if ( svcdata->type != NEBTYPE_SERVICECHECK_ASYNC_PRECHECK )
        return NEB_OK;

    /* get objects and set target function */
    if((svc=svcdata->object_ptr)==NULL) {
        gm_log( GM_LOG_ERROR, "Service handler received NULL service object pointer.\n" );
        return NEBERROR_CALLBACKCANCEL;
    }

    /* find the host associated with this service */
    if((hst=svc->host_ptr)==NULL) {
        gm_log( GM_LOG_ERROR, "Service handler received NULL host object pointer.\n" );
        return NEBERROR_CALLBACKCANCEL;
    }
    set_target_queue( hst, svc );

    /* local check? */
    if(!strcmp( target_queue, "" )) {
        gm_log( GM_LOG_DEBUG, "passing by local servicecheck: %s - %s\n", svcdata->host_name, svcdata->service_description);
        return NEB_OK;
    }

    gm_log( GM_LOG_DEBUG, "received job for queue %s: %s - %s\n", target_queue, svcdata->host_name, svcdata->service_description );

    temp_buffer[0]='\x0';

    /* as we have to intercept service checks so early
     * (we cannot cancel checks otherwise)
     * we have to do some service check logic here
     * taken from checks.c:
     */
    /* clear check options - we don't want old check options retained */
    svc->check_options=CHECK_OPTION_NONE;

    /* unset the freshening flag, otherwise only the first freshness check would be run */
    svc->is_being_freshened=FALSE;

    /* grab the host and service macro variables */
#ifdef USENAGIOS3
    clear_volatile_macros();
    grab_host_macros(hst);
    grab_service_macros(svc);
#endif
#if defined(USENAEMON) || defined(USENAGIOS4)
    memset(&mac, 0, sizeof(mac));
    clear_volatile_macros_r(&mac);
    grab_host_macros_r(&mac, hst);
    grab_service_macros_r(&mac, svc);
#endif

    /* get the raw command line */
#ifdef USENAGIOS3
    get_raw_command_line(svc->check_command_ptr,svc->service_check_command,&raw_command,0);
#endif
#if defined(USENAEMON) || defined(USENAGIOS4)
    get_raw_command_line_r(&mac, svc->check_command_ptr, svc->check_command, &raw_command, 0);
#endif
    if(raw_command==NULL){
        gm_log( GM_LOG_ERROR, "Raw check command for service '%s' on host '%s' was NULL - aborting.\n", svc->description, svc->host_name );
        return NEBERROR_CALLBACKCANCEL;
    }

    /* process any macros contained in the argument */
#ifdef USENAGIOS3
    process_macros(raw_command,&processed_command,0);
#endif
#if defined(USENAEMON) || defined(USENAGIOS4)
    process_macros_r(&mac, raw_command, &processed_command, 0);
#endif
    if(processed_command==NULL) {
        gm_log( GM_LOG_ERROR, "Processed check command for service '%s' on host '%s' was NULL - aborting.\n", svc->description, svc->host_name);
        my_free(raw_command);
        return NEBERROR_CALLBACKCANCEL;
    }
#if defined(USENAEMON)
    /* naemon sends unescaped newlines from ex.: the LONGPLUGINOUTPUT macro, so we have to escape
     * them ourselves: https://github.com/naemon/naemon-core/issues/153 */
    char *tmp = replace_str(processed_command, "\n", "\\n");
    free(processed_command);
    processed_command = tmp;
#endif

    /* log latency */
    if(mod_gm_opt->debug_level >= GM_LOG_DEBUG) {
        localtime_r(&svc->next_check, &next_check);
        strftime(buffer1, sizeof(buffer1), "%Y-%m-%d %H:%M:%S", &next_check );
        gm_log( GM_LOG_DEBUG, "service: '%s' - '%s', next_check is at %s, latency so far: %i\n", svcdata->host_name, svcdata->service_description, buffer1, ((int)core_time.tv_sec - (int)svc->next_check));
    }

    /* increment number of service checks that are currently running... */
    currently_running_service_checks++;

    /* set the execution flag */
    svc->is_executing=TRUE;

    gm_log( GM_LOG_TRACE, "cmd_line: %s\n", processed_command );

    snprintf( temp_buffer,GM_BUFFERSIZE-1,"type=service\nresult_queue=%s\nhost_name=%s\nservice_description=%s\nstart_time=%i.0\nnext_check=%i.0\ncore_time=%i.%i\ntimeout=%d\ncommand_line=%s\n\n\n",
              mod_gm_opt->result_queue,
              svcdata->host_name,
              svcdata->service_description,
              (int)svc->next_check,
              (int)svc->next_check,
              (int)core_time.tv_sec,
              (int)core_time.tv_usec,
              service_check_timeout,
              processed_command
            );

    uniq[0]='\x0';
    snprintf( uniq,GM_BUFFERSIZE-1,"%s-%s", svcdata->host_name, svcdata->service_description);

    /* execute forced checks with high prio as they are propably user requested */
#ifdef USENAGIOS3
    if(check_result_info.check_options & CHECK_OPTION_FORCE_EXECUTION)
#endif
#if defined(USENAEMON) || defined(USENAGIOS4)
    if(svc->check_options & CHECK_OPTION_FORCE_EXECUTION)
#endif
        prio = GM_JOB_PRIO_HIGH;

    if(add_job_to_queue( &client,
                         mod_gm_opt->server_list,
                         target_queue,
                        (mod_gm_opt->use_uniq_jobs == GM_ENABLED ? uniq : NULL),
                         temp_buffer,
                         prio,
                         GM_DEFAULT_JOB_RETRIES,
                         mod_gm_opt->transportmode,
                         TRUE
                        ) == GM_OK) {
        gm_log( GM_LOG_TRACE, "handle_svc_check() finished successfully\n" );
    }
    else {
        my_free(raw_command);
        my_free(processed_command);

        /* unset the execution flag */
        svc->is_executing=FALSE;

        /* decrement number of host checks that are currently running */
        currently_running_service_checks--;

        gm_log( GM_LOG_TRACE, "handle_svc_check() finished unsuccessfully\n" );
        return NEBERROR_CALLBACKCANCEL;
    }

    /* clean up */
    my_free(raw_command);
    my_free(processed_command);
#if defined(USENAEMON)
    clear_volatile_macros_r(&mac);
#endif
    
    /* orphaned check - submit fake result to mark service as orphaned */
#ifdef USENAGIOS
    if(mod_gm_opt->orphan_service_checks == GM_ENABLED && svc->check_options & CHECK_OPTION_ORPHAN_CHECK) {
        gm_log( GM_LOG_DEBUG, "service check for %s - %s orphaned\n", svc->host_name, svc->description );
        if ( ( chk_result = ( check_result * )gm_malloc( sizeof *chk_result ) ) == 0 )
            return NEBERROR_CALLBACKCANCEL;
        snprintf( temp_buffer,GM_BUFFERSIZE-1,"(service check orphaned, is the mod-gearman worker on queue '%s' running?)\n", target_queue);
        init_check_result(chk_result);
        chk_result->host_name           = gm_strdup( svc->host_name );
        chk_result->service_description = gm_strdup( svc->description );
        chk_result->scheduled_check     = TRUE;
        chk_result->reschedule_check    = TRUE;
        chk_result->output_file         = 0;
        chk_result->output_file_fp      = NULL;
        chk_result->output              = gm_strdup(temp_buffer);
        chk_result->return_code         = mod_gm_opt->orphan_return;
        chk_result->check_options       = CHECK_OPTION_NONE;
        chk_result->object_check_type   = SERVICE_CHECK;
        chk_result->check_type          = SERVICE_CHECK_ACTIVE;
        chk_result->start_time.tv_sec   = (unsigned long)time(NULL);
        chk_result->finish_time.tv_sec  = (unsigned long)time(NULL);
        chk_result->latency             = 0;
        mod_gm_add_result_to_list( chk_result );
        chk_result = NULL;
    }
#endif

    /* tell naemon to not execute */
    gm_log( GM_LOG_TRACE, "handle_svc_check() finished successfully -> %d\n", NEBERROR_CALLBACKOVERRIDE );

    return NEBERROR_CALLBACKOVERRIDE;
}


/* parse the module arguments */
static int read_arguments( const char *args_orig ) {
    int verify;
    int errors = 0;
    char *ptr;
    char *args;
    char *args_c;

    if (args_orig == NULL) {
        gm_log( GM_LOG_ERROR, "error parsing arguments: none provided.\n" );
        return GM_ERROR;
    }

    args = gm_strdup(args_orig);
    args_c = args;

    while ( (ptr = strsep( &args, " " )) != NULL ) {
        if(parse_args_line(mod_gm_opt, ptr, 0) != GM_OK) {
            errors++;
            break;
        }
    }

    verify = verify_options(mod_gm_opt);

    /* read keyfile */
    if(mod_gm_opt->keyfile != NULL && read_keyfile(mod_gm_opt) != GM_OK) {
        errors++;
    }

    if(mod_gm_opt->debug_level >= GM_LOG_DEBUG) {
        dumpconfig(mod_gm_opt, GM_NEB_MODE);
    }

    free(args_c);

    if(errors > 0) {
        return(GM_ERROR);
    }

    return(verify);
}


/* verify our option */
static int verify_options(mod_gm_opt_t *opt) {

    /* open new logfile */
    if ( opt->logmode == GM_LOG_MODE_AUTO && opt->logfile ) {
        opt->logmode = GM_LOG_MODE_FILE;
    }
    if(opt->logmode == GM_LOG_MODE_FILE && opt->logfile && opt->debug_level < GM_LOG_STDOUT) {
        opt->logfile_fp = fopen(opt->logfile, "a+");
        if(opt->logfile_fp == NULL) {
            gm_log( GM_LOG_ERROR, "error opening logfile: %s\n", opt->logfile );
        }
    }
    if ( opt->logmode == GM_LOG_MODE_AUTO ) {
        opt->logmode = GM_LOG_MODE_CORE;
    }

    /* did we get any server? */
    if(opt->server_num == 0) {
        gm_log( GM_LOG_ERROR, "please specify at least one server\n" );
        return(GM_ERROR);
    }

    if ( opt->result_queue == NULL )
        opt->result_queue = GM_DEFAULT_RESULT_QUEUE;

    /* nothing set by hand -> defaults */
    if( opt->set_queues_by_hand == 0 ) {
        gm_log( GM_LOG_DEBUG, "starting client with default queues\n" );
        opt->hosts    = GM_ENABLED;
        opt->services = GM_ENABLED;
        opt->events   = GM_ENABLED;
    }

    return(GM_OK);
}


/* return the prefered target function for our worker */
static void set_target_queue( host *hst, service *svc ) {
    int x=0;
    customvariablesmember *temp_customvariablesmember = NULL;

    /* empty our target */
    target_queue[0] = '\x0';

    /* grab target queue from custom variable */
    if( mod_gm_opt->queue_cust_var ) {
        if( svc ) {
            temp_customvariablesmember = svc->custom_variables;
            for(; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next) {
                if(!strcmp(mod_gm_opt->queue_cust_var, temp_customvariablesmember->variable_name)) {
                    if(!strcmp(temp_customvariablesmember->variable_value, "local")) {
                        gm_log( GM_LOG_TRACE, "bypassing local check from service custom variable\n" );
                        return;
                    }
                    gm_log( GM_LOG_TRACE, "got target queue from service custom variable: %s\n", temp_customvariablesmember->variable_value );
                    snprintf( target_queue, GM_BUFFERSIZE-1, "%s", temp_customvariablesmember->variable_value );
                    return;
                }
            }
        }

        /* search in host custom variables */
        temp_customvariablesmember = hst->custom_variables;
        for(; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next) {
            if(!strcmp(mod_gm_opt->queue_cust_var, temp_customvariablesmember->variable_name)) {
                if(!strcmp(temp_customvariablesmember->variable_value, "local")) {
                    gm_log( GM_LOG_TRACE, "bypassing local check from host custom variable\n" );
                    return;
                }
                gm_log( GM_LOG_TRACE, "got target queue from host custom variable: %s\n", temp_customvariablesmember->variable_value );
                snprintf( target_queue, GM_BUFFERSIZE-1, "%s", temp_customvariablesmember->variable_value );
                return;
            }
        }
    }

    /* look for matching local servicegroups */
    if ( svc ) {
        while ( mod_gm_opt->local_servicegroups_list[x] != NULL ) {
            servicegroup * temp_servicegroup = find_servicegroup( mod_gm_opt->local_servicegroups_list[x] );
            if ( temp_servicegroup != NULL && is_service_member_of_servicegroup( temp_servicegroup,svc )==TRUE ) {
                gm_log( GM_LOG_TRACE, "service is member of local servicegroup: %s\n", mod_gm_opt->local_servicegroups_list[x] );
                return;
            }
            x++;
        }
    }

    /* look for matching local hostgroups */
    x = 0;
    while ( mod_gm_opt->local_hostgroups_list[x] != NULL ) {
        hostgroup * temp_hostgroup = find_hostgroup( mod_gm_opt->local_hostgroups_list[x] );
        if ( temp_hostgroup != NULL && is_host_member_of_hostgroup( temp_hostgroup,hst )==TRUE ) {
            gm_log( GM_LOG_TRACE, "server is member of local hostgroup: %s\n", mod_gm_opt->local_hostgroups_list[x] );
            return;
        }
        x++;
    }

    /* look for matching servicegroups */
    x = 0;
    if ( svc ) {
        while ( mod_gm_opt->servicegroups_list[x] != NULL ) {
            servicegroup * temp_servicegroup = find_servicegroup( mod_gm_opt->servicegroups_list[x] );
            if ( temp_servicegroup != NULL && is_service_member_of_servicegroup( temp_servicegroup,svc )==TRUE ) {
                gm_log( GM_LOG_TRACE, "service is member of servicegroup: %s\n", mod_gm_opt->servicegroups_list[x] );
                snprintf( target_queue, GM_BUFFERSIZE-1, "servicegroup_%s", mod_gm_opt->servicegroups_list[x] );
                return;
            }
            x++;
        }
    }

    /* look for matching hostgroups */
    x = 0;
    while ( mod_gm_opt->hostgroups_list[x] != NULL ) {
        hostgroup * temp_hostgroup = find_hostgroup( mod_gm_opt->hostgroups_list[x] );
        if ( temp_hostgroup != NULL && is_host_member_of_hostgroup( temp_hostgroup,hst )==TRUE ) {
            gm_log( GM_LOG_TRACE, "server is member of hostgroup: %s\n", mod_gm_opt->hostgroups_list[x] );
            snprintf( target_queue, GM_BUFFERSIZE-1, "hostgroup_%s", mod_gm_opt->hostgroups_list[x] );
            return;
        }
        x++;
    }

    if ( svc ) {
        /* pass into the general service queue */
        if ( mod_gm_opt->services == GM_ENABLED && svc ) {
            snprintf( target_queue, GM_BUFFERSIZE-1, "service" );
            return;
        }
    }
    else {
        /* pass into the general host queue */
        if ( mod_gm_opt->hosts == GM_ENABLED ) {
            snprintf( target_queue, GM_BUFFERSIZE-1, "host" );
            return;
        }
    }

    return;
}


/* start our threads */
static void start_threads(void) {
    if ( result_threads_running < mod_gm_opt->result_workers ) {
        /* create result worker */
        int x;
        for(x = 0; x < mod_gm_opt->result_workers; x++) {
            result_threads_running++;
            pthread_create ( &result_thr[x], NULL, result_worker, (void *)&result_threads_running);
        }
    }
}


/* handle performance data */
int handle_perfdata(int event_type, void *data) {
    nebstruct_host_check_data *hostchkdata   = NULL;
    nebstruct_service_check_data *srvchkdata = NULL;
    host *hst        = NULL;
    service *svc     = NULL;
    int has_perfdata = FALSE;
    int i;
#if defined(USENAEMON) || defined(USENAGIOS4)
    char *perf_data;
#endif

    gm_log( GM_LOG_TRACE, "handle_perfdata(%d)\n", event_type );
    if(process_performance_data == 0) {
        gm_log( GM_LOG_TRACE, "handle_perfdata() process_performance_data disabled globally\n" );
        return 0;
    }

    /* what type of event/data do we have? */
    switch (event_type) {

        case NEBCALLBACK_HOST_CHECK_DATA:
            /* an aggregated status data dump just started or ended */
            if ((hostchkdata = (nebstruct_host_check_data *) data)) {

                if (hostchkdata->type != NEBTYPE_HOSTCHECK_PROCESSED || hostchkdata->perf_data == NULL ) {
                    break;
                }

                hst = (host *) hostchkdata->object_ptr;
                if(hst->process_performance_data == 0 && mod_gm_opt->perfdata_send_all == GM_DISABLED) {
                    gm_log( GM_LOG_TRACE, "handle_perfdata() process_performance_data disabled for: %s\n", hst->name );
                    break;
                }

                uniq[0]='\x0';
                snprintf( uniq,GM_BUFFERSIZE-1,"%s", hostchkdata->host_name);

#if defined(USENAEMON) || defined(USENAGIOS4)
                /* replace newlines with actual newlines */
                perf_data = replace_str(hostchkdata->perf_data, "\\n", "\n");
#endif


                temp_buffer[0]='\x0';
                snprintf( temp_buffer,GM_BUFFERSIZE-1,
                            "DATATYPE::HOSTPERFDATA\t"
                            "TIMET::%d\t"
                            "HOSTNAME::%s\t"
                            "HOSTPERFDATA::%s\t"
                            "HOSTCHECKCOMMAND::%s!%s\t"
                            "HOSTSTATE::%d\t"
                            "HOSTSTATETYPE::%d\n"
                            "HOSTINTERVAL::%f\n\n",
                            (int)hostchkdata->timestamp.tv_sec,
#ifdef USENAGIOS3
                            hostchkdata->host_name, hostchkdata->perf_data,
#endif
#if defined(USENAEMON) || defined(USENAGIOS4)
                            hostchkdata->host_name, perf_data,
#endif
                            hostchkdata->command_name, hostchkdata->command_args,
                            hostchkdata->state, hostchkdata->state_type,
                            hst->check_interval);
                has_perfdata = TRUE;
#if defined(USENAEMON) || defined(USENAGIOS4)
                free(perf_data);
#endif

            }
            break;

        case NEBCALLBACK_SERVICE_CHECK_DATA:
            /* an aggregated status data dump just started or ended */
            if ((srvchkdata = (nebstruct_service_check_data *) data)) {

                if(srvchkdata->type != NEBTYPE_SERVICECHECK_PROCESSED || srvchkdata->perf_data == NULL) {
                    break;
                }

                /* find the naemon service object for this service */
                svc = (service *) srvchkdata->object_ptr;
                if(svc->process_performance_data == 0 && mod_gm_opt->perfdata_send_all == GM_DISABLED) {
                    gm_log( GM_LOG_TRACE, "handle_perfdata() process_performance_data disabled for: %s - %s\n", svc->host_name, svc->description );
                    break;
                }

                uniq[0]='\x0';
                snprintf( uniq,GM_BUFFERSIZE-1,"%s-%s", srvchkdata->host_name, srvchkdata->service_description);

#if defined(USENAEMON) || defined(USENAGIOS4)
                /* replace newlines with actual newlines */
                perf_data = replace_str(srvchkdata->perf_data, "\\n", "\n");
#endif

                temp_buffer[0]='\x0';
                snprintf( temp_buffer,GM_BUFFERSIZE-1,
                            "DATATYPE::SERVICEPERFDATA\t"
                            "TIMET::%d\t"
                            "HOSTNAME::%s\t"
                            "SERVICEDESC::%s\t"
                            "SERVICEPERFDATA::%s\t"
                            "SERVICECHECKCOMMAND::%s\t"
                            "SERVICESTATE::%d\t"
                            "SERVICESTATETYPE::%d\n"
                            "SERVICEINTERVAL::%f\n\n",
                            (int)srvchkdata->timestamp.tv_sec,
                            srvchkdata->host_name, srvchkdata->service_description,
#ifdef USENAGIOS3
                            srvchkdata->perf_data, svc->service_check_command,
#endif
#if defined(USENAEMON) || defined(USENAGIOS4)
                            perf_data, svc->check_command,
#endif
                            srvchkdata->state, srvchkdata->state_type,
                            svc->check_interval);
                temp_buffer[GM_BUFFERSIZE-1]='\x0';
                has_perfdata = TRUE;
#if defined(USENAEMON) || defined(USENAGIOS4)
                free(perf_data);
#endif
            }
            break;

        default:
            break;
    }

    if(has_perfdata == TRUE) {
        for (i = 0; i < mod_gm_opt->perfdata_queues_num; i++) {
            char *perfdata_queue = mod_gm_opt->perfdata_queues_list[i];
            /* add our job onto the queue */
            if(add_job_to_queue( &client,
                                 mod_gm_opt->server_list,
                                 perfdata_queue,
                                 (mod_gm_opt->perfdata_mode == GM_PERFDATA_OVERWRITE ? uniq : NULL),
                                 temp_buffer,
                                 GM_JOB_PRIO_NORMAL,
                                 GM_DEFAULT_JOB_RETRIES,
                                 mod_gm_opt->transportmode,
                                 TRUE
                                ) == GM_OK) {
                gm_log( GM_LOG_TRACE, "handle_perfdata() successfully added data to %s\n", perfdata_queue );
            }
            else {
                gm_log( GM_LOG_TRACE, "handle_perfdata() failed to add data to %s\n", perfdata_queue );
            }
        }
    }

    return 0;
}


/* handle generic exports */
int handle_export(int callback_type, void *data) {
    int i, debug_level_orig, return_code;
    char * buffer;
    char * type;
    char * event_type;
    nebstruct_log_data          * nld;
    nebstruct_process_data      * npd;
    nebstruct_timed_event_data  * nted;

    temp_buffer[0]          = '\x0';
    mod_gm_opt->debug_level = -1;
    debug_level_orig    = mod_gm_opt->debug_level;
    return_code         = 0;

    /* what type of event/data do we have? */
    switch (callback_type) {
        case NEBCALLBACK_PROCESS_DATA:                      /*  7 */
            npd    = (nebstruct_process_data *)data;
            type   = nebtype2str(npd->type);
            snprintf( temp_buffer,GM_BUFFERSIZE-1, "{\"callback_type\":\"%s\",\"type\":\"%s\",\"flags\":%d,\"attr\":%d,\"timestamp\":%d.%d}",
                    "NEBCALLBACK_PROCESS_DATA",
                    type,
                    npd->flags,
                    npd->attr,
                    (int)npd->timestamp.tv_sec, (int)npd->timestamp.tv_usec
                    );
            free(type);
            break;
        case NEBCALLBACK_TIMED_EVENT_DATA:                  /*  8 */
            nted       = (nebstruct_timed_event_data *)data;
            event_type = eventtype2str(nted->event_type);
            type       = nebtype2str(nted->type);
            snprintf( temp_buffer,GM_BUFFERSIZE-1, "{\"callback_type\":\"%s\",\"event_type\":\"%s\",\"type\":\"%s\",\"flags\":%d,\"attr\":%d,\"timestamp\":%d.%d,\"recurring\":%d,\"run_time\":%d}",
                    "NEBCALLBACK_TIMED_EVENT_DATA",
                    event_type,
                    type,
                    nted->flags,
                    nted->attr,
                    (int)nted->timestamp.tv_sec, (int)nted->timestamp.tv_usec,
                    nted->recurring,
                    (int)nted->run_time
                    );
            free(event_type);
            free(type);
            break;
        case NEBCALLBACK_LOG_DATA:                          /*  9 */
            nld    = (nebstruct_log_data *)data;
            buffer = escapestring(nld->data);
            type   = nebtype2str(nld->type);
            snprintf( temp_buffer,GM_BUFFERSIZE-1, "{\"callback_type\":\"%s\",\"type\":\"%s\",\"flags\":%d,\"attr\":%d,\"timestamp\":%d.%d,\"entry_time\":%d,\"data_type\":%d,\"data\":\"%s\"}",
                    "NEBCALLBACK_LOG_DATA",
                    type,
                    nld->flags,
                    nld->attr,
                    (int)nld->timestamp.tv_sec, (int)nld->timestamp.tv_usec,
                    (int)nld->entry_time,
                    nld->data_type,
                    buffer);
            free(type);
            free(buffer);
            break;
        case NEBCALLBACK_SYSTEM_COMMAND_DATA:               /* 10 */
            break;
        case NEBCALLBACK_EVENT_HANDLER_DATA:                /* 11 */
            break;
        case NEBCALLBACK_NOTIFICATION_DATA:                 /* 12 */
            break;
        case NEBCALLBACK_SERVICE_CHECK_DATA:                /* 13 */
            break;
        case NEBCALLBACK_HOST_CHECK_DATA:                   /* 14 */
            break;
        case NEBCALLBACK_COMMENT_DATA:                      /* 15 */
            break;
        case NEBCALLBACK_DOWNTIME_DATA:                     /* 16 */
            break;
        case NEBCALLBACK_FLAPPING_DATA:                     /* 17 */
            break;
        case NEBCALLBACK_PROGRAM_STATUS_DATA:               /* 18 */
            break;
        case NEBCALLBACK_HOST_STATUS_DATA:                  /* 19 */
            break;
        case NEBCALLBACK_SERVICE_STATUS_DATA:               /* 20 */
            break;
        case NEBCALLBACK_ADAPTIVE_PROGRAM_DATA:             /* 21 */
            break;
        case NEBCALLBACK_ADAPTIVE_HOST_DATA:                /* 22 */
            break;
        case NEBCALLBACK_ADAPTIVE_SERVICE_DATA:             /* 23 */
            break;
        case NEBCALLBACK_EXTERNAL_COMMAND_DATA:             /* 24 */
            break;
        case NEBCALLBACK_AGGREGATED_STATUS_DATA:            /* 25 */
            break;
        case NEBCALLBACK_RETENTION_DATA:                    /* 26 */
            break;
        case NEBCALLBACK_CONTACT_NOTIFICATION_DATA:         /* 27 */
            break;
        case NEBCALLBACK_CONTACT_NOTIFICATION_METHOD_DATA:  /* 28 */
            break;
        case NEBCALLBACK_ACKNOWLEDGEMENT_DATA:              /* 29 */
            break;
        case NEBCALLBACK_STATE_CHANGE_DATA:                 /* 30 */
            break;
        case NEBCALLBACK_CONTACT_STATUS_DATA:               /* 31 */
            break;
        case NEBCALLBACK_ADAPTIVE_CONTACT_DATA:             /* 32 */
            break;
        default:
            gm_log( GM_LOG_ERROR, "handle_export() unknown export type: %d\n", callback_type );
            mod_gm_opt->debug_level = debug_level_orig;
            return 0;
    }

    if(temp_buffer[0] != '\x0') {

        for(i=0;i<mod_gm_opt->exports[callback_type]->elem_number;i++) {
            return_code = mod_gm_opt->exports[callback_type]->return_code[i];
            add_job_to_queue( &client,
                              mod_gm_opt->server_list,
                              mod_gm_opt->exports[callback_type]->name[i], /* queue name */
                              NULL,
                              temp_buffer,
                              GM_JOB_PRIO_NORMAL,
                              GM_DEFAULT_JOB_RETRIES,
                              mod_gm_opt->transportmode,
                              send_now
                            );
        }
    }

    mod_gm_opt->debug_level = debug_level_orig;
    return return_code;
}


/* core log wrapper */
void write_core_log(char *data) {
#ifdef USENAEMON
    nm_log( NSLOG_INFO_MESSAGE, data );
#endif
#ifdef USENAGIOS
    write_to_all_logs( data, NSLOG_INFO_MESSAGE );
#endif
    return;
}


/* return human readable name for neb type */
char * nebtype2str(int i) {
    switch(i) {
        case NEBTYPE_NONE:
            return gm_strdup("NEBTYPE_NONE"); break;
        case NEBTYPE_HELLO:
            return gm_strdup("NEBTYPE_HELLO"); break;
        case NEBTYPE_GOODBYE:
            return gm_strdup("NEBTYPE_GOODBYE"); break;
        case NEBTYPE_INFO:
            return gm_strdup("NEBTYPE_INFO"); break;
        case NEBTYPE_PROCESS_START:
            return gm_strdup("NEBTYPE_PROCESS_START"); break;
        case NEBTYPE_PROCESS_DAEMONIZE:
            return gm_strdup("NEBTYPE_PROCESS_DAEMONIZE"); break;
        case NEBTYPE_PROCESS_RESTART:
            return gm_strdup("NEBTYPE_PROCESS_RESTART"); break;
        case NEBTYPE_PROCESS_SHUTDOWN:
            return gm_strdup("NEBTYPE_PROCESS_SHUTDOWN"); break;
        case NEBTYPE_PROCESS_PRELAUNCH:
            return gm_strdup("NEBTYPE_PROCESS_PRELAUNCH"); break;
        case NEBTYPE_PROCESS_EVENTLOOPSTART:
            return gm_strdup("NEBTYPE_PROCESS_EVENTLOOPSTART"); break;
        case NEBTYPE_PROCESS_EVENTLOOPEND:
            return gm_strdup("NEBTYPE_PROCESS_EVENTLOOPEND"); break;
        case NEBTYPE_TIMEDEVENT_ADD:
            return gm_strdup("NEBTYPE_TIMEDEVENT_ADD"); break;
        case NEBTYPE_TIMEDEVENT_REMOVE:
            return gm_strdup("NEBTYPE_TIMEDEVENT_REMOVE"); break;
        case NEBTYPE_TIMEDEVENT_EXECUTE:
            return gm_strdup("NEBTYPE_TIMEDEVENT_EXECUTE"); break;
        case NEBTYPE_TIMEDEVENT_DELAY:
            return gm_strdup("NEBTYPE_TIMEDEVENT_DELAY"); break;
        case NEBTYPE_TIMEDEVENT_SKIP:
            return gm_strdup("NEBTYPE_TIMEDEVENT_SKIP"); break;
        case NEBTYPE_TIMEDEVENT_SLEEP:
            return gm_strdup("NEBTYPE_TIMEDEVENT_SLEEP"); break;
        case NEBTYPE_LOG_DATA:
            return gm_strdup("NEBTYPE_LOG_DATA"); break;
        case NEBTYPE_LOG_ROTATION:
            return gm_strdup("NEBTYPE_LOG_ROTATION"); break;
        case NEBTYPE_SYSTEM_COMMAND_START:
            return gm_strdup("NEBTYPE_SYSTEM_COMMAND_START"); break;
        case NEBTYPE_SYSTEM_COMMAND_END:
            return gm_strdup("NEBTYPE_SYSTEM_COMMAND_END"); break;
        case NEBTYPE_EVENTHANDLER_START:
            return gm_strdup("NEBTYPE_EVENTHANDLER_START"); break;
        case NEBTYPE_EVENTHANDLER_END:
            return gm_strdup("NEBTYPE_EVENTHANDLER_END"); break;
        case NEBTYPE_NOTIFICATION_START:
            return gm_strdup("NEBTYPE_NOTIFICATION_START"); break;
        case NEBTYPE_NOTIFICATION_END:
            return gm_strdup("NEBTYPE_NOTIFICATION_END"); break;
        case NEBTYPE_CONTACTNOTIFICATION_START:
            return gm_strdup("NEBTYPE_CONTACTNOTIFICATION_START"); break;
        case NEBTYPE_CONTACTNOTIFICATION_END:
            return gm_strdup("NEBTYPE_CONTACTNOTIFICATION_END"); break;
        case NEBTYPE_CONTACTNOTIFICATIONMETHOD_START:
            return gm_strdup("NEBTYPE_CONTACTNOTIFICATIONMETHOD_START"); break;
        case NEBTYPE_CONTACTNOTIFICATIONMETHOD_END:
            return gm_strdup("NEBTYPE_CONTACTNOTIFICATIONMETHOD_END"); break;
        case NEBTYPE_SERVICECHECK_INITIATE:
            return gm_strdup("NEBTYPE_SERVICECHECK_INITIATE"); break;
        case NEBTYPE_SERVICECHECK_PROCESSED:
            return gm_strdup("NEBTYPE_SERVICECHECK_PROCESSED"); break;
        case NEBTYPE_SERVICECHECK_RAW_START:
            return gm_strdup("NEBTYPE_SERVICECHECK_RAW_START"); break;
        case NEBTYPE_SERVICECHECK_RAW_END:
            return gm_strdup("NEBTYPE_SERVICECHECK_RAW_END"); break;
        case NEBTYPE_SERVICECHECK_ASYNC_PRECHECK:
            return gm_strdup("NEBTYPE_SERVICECHECK_ASYNC_PRECHECK"); break;
        case NEBTYPE_HOSTCHECK_INITIATE:
            return gm_strdup("NEBTYPE_HOSTCHECK_INITIATE"); break;
        case NEBTYPE_HOSTCHECK_PROCESSED:
            return gm_strdup("NEBTYPE_HOSTCHECK_PROCESSED"); break;
        case NEBTYPE_HOSTCHECK_RAW_START:
            return gm_strdup("NEBTYPE_HOSTCHECK_RAW_START"); break;
        case NEBTYPE_HOSTCHECK_RAW_END:
            return gm_strdup("NEBTYPE_HOSTCHECK_RAW_END"); break;
        case NEBTYPE_HOSTCHECK_ASYNC_PRECHECK:
            return gm_strdup("NEBTYPE_HOSTCHECK_ASYNC_PRECHECK"); break;
        case NEBTYPE_HOSTCHECK_SYNC_PRECHECK:
            return gm_strdup("NEBTYPE_HOSTCHECK_SYNC_PRECHECK"); break;
        case NEBTYPE_COMMENT_ADD:
            return gm_strdup("NEBTYPE_COMMENT_ADD"); break;
        case NEBTYPE_COMMENT_DELETE:
            return gm_strdup("NEBTYPE_COMMENT_DELETE"); break;
        case NEBTYPE_COMMENT_LOAD:
            return gm_strdup("NEBTYPE_COMMENT_LOAD"); break;
        case NEBTYPE_FLAPPING_START:
            return gm_strdup("NEBTYPE_FLAPPING_START"); break;
        case NEBTYPE_FLAPPING_STOP:
            return gm_strdup("NEBTYPE_FLAPPING_STOP"); break;
        case NEBTYPE_DOWNTIME_ADD:
            return gm_strdup("NEBTYPE_DOWNTIME_ADD"); break;
        case NEBTYPE_DOWNTIME_DELETE:
            return gm_strdup("NEBTYPE_DOWNTIME_DELETE"); break;
        case NEBTYPE_DOWNTIME_LOAD:
            return gm_strdup("NEBTYPE_DOWNTIME_LOAD"); break;
        case NEBTYPE_DOWNTIME_START:
            return gm_strdup("NEBTYPE_DOWNTIME_START"); break;
        case NEBTYPE_DOWNTIME_STOP:
            return gm_strdup("NEBTYPE_DOWNTIME_STOP"); break;
        case NEBTYPE_PROGRAMSTATUS_UPDATE:
            return gm_strdup("NEBTYPE_PROGRAMSTATUS_UPDATE"); break;
        case NEBTYPE_HOSTSTATUS_UPDATE:
            return gm_strdup("NEBTYPE_HOSTSTATUS_UPDATE"); break;
        case NEBTYPE_SERVICESTATUS_UPDATE:
            return gm_strdup("NEBTYPE_SERVICESTATUS_UPDATE"); break;
        case NEBTYPE_CONTACTSTATUS_UPDATE:
            return gm_strdup("NEBTYPE_CONTACTSTATUS_UPDATE"); break;
        case NEBTYPE_ADAPTIVEPROGRAM_UPDATE:
            return gm_strdup("NEBTYPE_ADAPTIVEPROGRAM_UPDATE"); break;
        case NEBTYPE_ADAPTIVEHOST_UPDATE:
            return gm_strdup("NEBTYPE_ADAPTIVEHOST_UPDATE"); break;
        case NEBTYPE_ADAPTIVESERVICE_UPDATE:
            return gm_strdup("NEBTYPE_ADAPTIVESERVICE_UPDATE"); break;
        case NEBTYPE_ADAPTIVECONTACT_UPDATE:
            return gm_strdup("NEBTYPE_ADAPTIVECONTACT_UPDATE"); break;
        case NEBTYPE_EXTERNALCOMMAND_START:
            return gm_strdup("NEBTYPE_EXTERNALCOMMAND_START"); break;
        case NEBTYPE_EXTERNALCOMMAND_END:
            return gm_strdup("NEBTYPE_EXTERNALCOMMAND_END"); break;
        case NEBTYPE_AGGREGATEDSTATUS_STARTDUMP:
            return gm_strdup("NEBTYPE_AGGREGATEDSTATUS_STARTDUMP"); break;
        case NEBTYPE_AGGREGATEDSTATUS_ENDDUMP:
            return gm_strdup("NEBTYPE_AGGREGATEDSTATUS_ENDDUMP"); break;
        case NEBTYPE_RETENTIONDATA_STARTLOAD:
            return gm_strdup("NEBTYPE_RETENTIONDATA_STARTLOAD"); break;
        case NEBTYPE_RETENTIONDATA_ENDLOAD:
            return gm_strdup("NEBTYPE_RETENTIONDATA_ENDLOAD"); break;
        case NEBTYPE_RETENTIONDATA_STARTSAVE:
            return gm_strdup("NEBTYPE_RETENTIONDATA_STARTSAVE"); break;
        case NEBTYPE_RETENTIONDATA_ENDSAVE:
            return gm_strdup("NEBTYPE_RETENTIONDATA_ENDSAVE"); break;
        case NEBTYPE_ACKNOWLEDGEMENT_ADD:
            return gm_strdup("NEBTYPE_ACKNOWLEDGEMENT_ADD"); break;
        case NEBTYPE_ACKNOWLEDGEMENT_REMOVE:
            return gm_strdup("NEBTYPE_ACKNOWLEDGEMENT_REMOVE"); break;
        case NEBTYPE_ACKNOWLEDGEMENT_LOAD:
            return gm_strdup("NEBTYPE_ACKNOWLEDGEMENT_LOAD"); break;
        case NEBTYPE_STATECHANGE_START:
            return gm_strdup("NEBTYPE_STATECHANGE_START"); break;
        case NEBTYPE_STATECHANGE_END:
            return gm_strdup("NEBTYPE_STATECHANGE_END"); break;
    }
    return gm_strdup("UNKNOWN");
}


/* return human readable name for nebcallback */
char * nebcallback2str(int i) {
    switch(i) {
        case NEBCALLBACK_PROCESS_DATA:
            return gm_strdup("NEBCALLBACK_PROCESS_DATA"); break;
        case NEBCALLBACK_TIMED_EVENT_DATA:
            return gm_strdup("NEBCALLBACK_TIMED_EVENT_DATA"); break;
        case NEBCALLBACK_LOG_DATA:
            return gm_strdup("NEBCALLBACK_LOG_DATA"); break;
        case NEBCALLBACK_SYSTEM_COMMAND_DATA:
            return gm_strdup("NEBCALLBACK_SYSTEM_COMMAND_DATA"); break;
        case NEBCALLBACK_EVENT_HANDLER_DATA:
            return gm_strdup("NEBCALLBACK_EVENT_HANDLER_DATA"); break;
        case NEBCALLBACK_NOTIFICATION_DATA:
            return gm_strdup("NEBCALLBACK_NOTIFICATION_DATA"); break;
        case NEBCALLBACK_SERVICE_CHECK_DATA:
            return gm_strdup("NEBCALLBACK_SERVICE_CHECK_DATA"); break;
        case NEBCALLBACK_HOST_CHECK_DATA:
            return gm_strdup("NEBCALLBACK_HOST_CHECK_DATA"); break;
        case NEBCALLBACK_COMMENT_DATA:
            return gm_strdup("NEBCALLBACK_COMMENT_DATA"); break;
        case NEBCALLBACK_DOWNTIME_DATA:
            return gm_strdup("NEBCALLBACK_DOWNTIME_DATA"); break;
        case NEBCALLBACK_FLAPPING_DATA:
            return gm_strdup("NEBCALLBACK_FLAPPING_DATA"); break;
        case NEBCALLBACK_PROGRAM_STATUS_DATA:
            return gm_strdup("NEBCALLBACK_PROGRAM_STATUS_DATA"); break;
        case NEBCALLBACK_HOST_STATUS_DATA:
            return gm_strdup("NEBCALLBACK_HOST_STATUS_DATA"); break;
        case NEBCALLBACK_SERVICE_STATUS_DATA:
            return gm_strdup("NEBCALLBACK_SERVICE_STATUS_DATA"); break;
        case NEBCALLBACK_ADAPTIVE_PROGRAM_DATA:
            return gm_strdup("NEBCALLBACK_ADAPTIVE_PROGRAM_DATA"); break;
        case NEBCALLBACK_ADAPTIVE_HOST_DATA:
            return gm_strdup("NEBCALLBACK_ADAPTIVE_HOST_DATA"); break;
        case NEBCALLBACK_ADAPTIVE_SERVICE_DATA:
            return gm_strdup("NEBCALLBACK_ADAPTIVE_SERVICE_DATA"); break;
        case NEBCALLBACK_EXTERNAL_COMMAND_DATA:
            return gm_strdup("NEBCALLBACK_EXTERNAL_COMMAND_DATA"); break;
        case NEBCALLBACK_AGGREGATED_STATUS_DATA:
            return gm_strdup("NEBCALLBACK_AGGREGATED_STATUS_DATA"); break;
        case NEBCALLBACK_RETENTION_DATA:
            return gm_strdup("NEBCALLBACK_RETENTION_DATA"); break;
        case NEBCALLBACK_CONTACT_NOTIFICATION_DATA:
            return gm_strdup("NEBCALLBACK_CONTACT_NOTIFICATION_DATA"); break;
        case NEBCALLBACK_CONTACT_NOTIFICATION_METHOD_DATA:
            return gm_strdup("NEBCALLBACK_CONTACT_NOTIFICATION_METHOD_DATA"); break;
        case NEBCALLBACK_ACKNOWLEDGEMENT_DATA:
            return gm_strdup("NEBCALLBACK_ACKNOWLEDGEMENT_DATA"); break;
        case NEBCALLBACK_STATE_CHANGE_DATA:
            return gm_strdup("NEBCALLBACK_STATE_CHANGE_DATA"); break;
        case NEBCALLBACK_CONTACT_STATUS_DATA:
            return gm_strdup("NEBCALLBACK_CONTACT_STATUS_DATA"); break;
        case NEBCALLBACK_ADAPTIVE_CONTACT_DATA:
            return gm_strdup("NEBCALLBACK_ADAPTIVE_CONTACT_DATA"); break;
    }
    return gm_strdup("UNKNOWN");
}

/* return human readable name for eventtype */
char * eventtype2str(int i) {
    switch(i) {
        case 0:
            return gm_strdup("EVENT_SERVICE_CHECK"); break;
        case 1:
            return gm_strdup("EVENT_COMMAND_CHECK"); break;
        case 2:
            return gm_strdup("EVENT_LOG_ROTATION"); break;
        case 3:
            return gm_strdup("EVENT_PROGRAM_SHUTDOWN"); break;
        case 4:
            return gm_strdup("EVENT_PROGRAM_RESTART"); break;
        case 5:
            return gm_strdup("EVENT_CHECK_REAPER"); break;
        case 6:
            return gm_strdup("EVENT_ORPHAN_CHECK"); break;
        case 7:
            return gm_strdup("EVENT_RETENTION_SAVE"); break;
        case 8:
            return gm_strdup("EVENT_STATUS_SAVE"); break;
        case 9:
            return gm_strdup("EVENT_SCHEDULED_DOWNTIME"); break;
        case 10:
            return gm_strdup("EVENT_SFRESHNESS_CHECK"); break;
        case 11:
            return gm_strdup("EVENT_EXPIRE_DOWNTIME"); break;
        case 12:
            return gm_strdup("EVENT_HOST_CHECK"); break;
        case 13:
            return gm_strdup("EVENT_HFRESHNESS_CHECK"); break;
        case 14:
            return gm_strdup("EVENT_RESCHEDULE_CHECKS"); break;
        case 15:
            return gm_strdup("EVENT_EXPIRE_COMMENT"); break;
        case 16:
            return gm_strdup("EVENT_CHECK_PROGRAM_UPDATE"); break;
        case 98:
            return gm_strdup("EVENT_SLEEP"); break;
        case 99:
            return gm_strdup("EVENT_USER_FUNCTION"); break;
    }
    return gm_strdup("UNKNOWN");
}
