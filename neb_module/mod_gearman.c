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
#include "utils.h"
#include "result_thread.h"
#include "mod_gearman.h"
#include "gm_log.h"
#include "gearman.h"

/* specify event broker API version (required) */
NEB_API_VERSION( CURRENT_NEB_API_VERSION )

/* import some global variables */
extern int            event_broker_options;
extern int            currently_running_host_checks;
extern int            currently_running_service_checks;
extern int            service_check_timeout;
extern int            host_check_timeout;
extern timed_event  * event_list_low;
extern timed_event  * event_list_low_tail;
extern int            process_performance_data;
extern check_result   check_result_info;
extern check_result * check_result_list;

/* global variables */
static check_result * mod_gm_result_list = 0;
static pthread_mutex_t mod_gm_result_list_mutex = PTHREAD_MUTEX_INITIALIZER;
void *gearman_module_handle=NULL;
gearman_client_st client;

int send_now               = FALSE;
int result_threads_running = 0;
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
static int   handle_timed_events( int, void * );
static void  start_threads(void);
static check_result * merge_result_lists(check_result * lista, check_result * listb);
static void move_results_to_core(void);

int nebmodule_init( int flags, char *args, nebmodule *handle ) {
    int i;
    int broker_option_errors = 0;

    /* save our handle */
    gearman_module_handle=handle;

    /* set some module info */
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_TITLE,   "mod_gearman" );
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_AUTHOR,  "Sven Nierlein" );
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_TITLE,   "Copyright (c) 2010 Sven Nierlein" );
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_VERSION, GM_VERSION );
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_LICENSE, "GPL v3" );
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_DESC,    "distribute host/service checks and eventhandler via gearman" );

    mod_gm_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);
    gm_log( GM_LOG_INFO,  "Version %s\n", GM_VERSION );

    /* parse arguments */
    read_arguments( args );
    gm_log( GM_LOG_TRACE, "args: %s\n", args );
    gm_log( GM_LOG_TRACE, "nebmodule_init(%i, %i)\n", flags );
    gm_log( GM_LOG_DEBUG, "running on libgearman %s\n", gearman_version() );

    /* check for minimum eventbroker options */
    if(!(event_broker_options & BROKER_PROGRAM_STATE)) {
        gm_log( GM_LOG_ERROR, "mod_gearman needs BROKER_PROGRAM_STATE (%i) event_broker_options enabled to work\n", BROKER_PROGRAM_STATE );
        broker_option_errors++;
    }
    if(!(event_broker_options & BROKER_TIMED_EVENTS)) {
        gm_log( GM_LOG_ERROR, "mod_gearman needs BROKER_TIMED_EVENTS (%i) event_broker_options enabled to work\n", BROKER_TIMED_EVENTS );
        broker_option_errors++;
    }
    if(    (    mod_gm_opt->perfdata == GM_ENABLED
             || mod_gm_opt->hostgroups_num > 0
             || mod_gm_opt->hosts == GM_ENABLED
           )
        && !(event_broker_options & BROKER_HOST_CHECKS)) {
        gm_log( GM_LOG_ERROR, "mod_gearman needs BROKER_HOST_CHECKS (%i) event_broker_options enabled to work\n", BROKER_HOST_CHECKS );
        broker_option_errors++;
    }
    if(    (    mod_gm_opt->perfdata == GM_ENABLED
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
    neb_register_callback( NEBCALLBACK_TIMED_EVENT_DATA, gearman_module_handle, 0, handle_timed_events );

    /* register export callbacks */
    for(i=0;i<=GM_NEBTYPESSIZE;i++) {
        if(mod_gm_opt->exports[i]->elem_number > 0)
            neb_register_callback( i, gearman_module_handle, 0, handle_export );
    }

    gm_log( GM_LOG_DEBUG, "finished initializing\n" );

    return NEB_OK;
}


/* register eventhandler callback */
static void register_neb_callbacks(void) {

    /* only if we have hostgroups defined or general hosts enabled */
    if ( mod_gm_opt->do_hostchecks == GM_ENABLED && ( mod_gm_opt->hostgroups_num > 0 || mod_gm_opt->hosts == GM_ENABLED ))
        neb_register_callback( NEBCALLBACK_HOST_CHECK_DATA,    gearman_module_handle, 0, handle_host_check );

    /* only if we have groups defined or general services enabled */
    if ( mod_gm_opt->servicegroups_num > 0 || mod_gm_opt->hostgroups_num > 0 || mod_gm_opt->services == GM_ENABLED )
        neb_register_callback( NEBCALLBACK_SERVICE_CHECK_DATA, gearman_module_handle, 0, handle_svc_check );

    if ( mod_gm_opt->events == GM_ENABLED )
        neb_register_callback( NEBCALLBACK_EVENT_HANDLER_DATA, gearman_module_handle, 0, handle_eventhandler );

    if ( mod_gm_opt->perfdata == GM_ENABLED ) {
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

    if ( mod_gm_opt->perfdata == GM_ENABLED ) {
        neb_deregister_callback( NEBCALLBACK_HOST_CHECK_DATA, gearman_module_handle );
        neb_deregister_callback( NEBCALLBACK_SERVICE_CHECK_DATA, gearman_module_handle );
    }

    /* register export callbacks */
    for(x=0;x<=GM_NEBTYPESSIZE;x++) {
        if(mod_gm_opt->exports[x]->elem_number > 0)
            neb_deregister_callback( x, gearman_module_handle );
    }

    neb_deregister_callback( NEBCALLBACK_PROCESS_DATA, gearman_module_handle );

    gm_log( GM_LOG_DEBUG, "deregistered callbacks\n" );

    /* stop result threads */
    for(x = 0; x < mod_gm_opt->result_workers; x++) {
        pthread_cancel(result_thr[x]);
        pthread_join(result_thr[x], NULL);
    }

    /* cleanup client */
    free_client(&client);

    return NEB_OK;
}


/* handle timed events */
static int handle_timed_events( int event_type, void *data ) {
    nebstruct_timed_event_data * ted = (nebstruct_timed_event_data *)data;

    /* sanity checks */
    if (event_type != NEBCALLBACK_TIMED_EVENT_DATA || ted == 0)
        return NEB_ERROR;

    /* we only care about REAPER events */
    if (ted->event_type != EVENT_CHECK_REAPER)
        return NEB_OK;

    gm_log( GM_LOG_TRACE, "handle_timed_events(%i, data)\n", event_type, ted->event_type );

    move_results_to_core();

    return NEB_OK;
}


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


/* insert results list into core */
static void move_results_to_core() {
   check_result * local;

   /* safely save off currently local list */
   pthread_mutex_lock(&mod_gm_result_list_mutex);
   local = mod_gm_result_list;
   mod_gm_result_list = 0;
   pthread_mutex_unlock(&mod_gm_result_list_mutex);

   /* merge local into check_result_list, store in check_result_list */
   check_result_list = merge_result_lists(local, check_result_list);
}


/* add list to gearman result list */
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
         * this cannot be done befor nagios has finished reading his config
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

    gm_log( GM_LOG_DEBUG, "got eventhandler event\n" );
    gm_log( GM_LOG_TRACE, "handle_eventhandler(%i, data)\n", event_type );

    if ( event_type != NEBTYPE_EVENTHANDLER_START )
        return NEB_OK;

    ds = ( nebstruct_event_handler_data * )data;

    gm_log( GM_LOG_TRACE, "got eventhandler event: %s\n", ds->command_line );

    temp_buffer[0]='\x0';
    snprintf( temp_buffer,sizeof( temp_buffer )-1,"type=eventhandler\ncommand_line=%s\n\n\n",ds->command_line );
    temp_buffer[sizeof( temp_buffer )-1]='\x0';

    if(add_job_to_queue( &client,
                         mod_gm_opt->server_list,
                         "eventhandler",
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

    /* tell nagios to not execute */
    return NEBERROR_CALLBACKOVERRIDE;
}


/* handle host check events */
static int handle_host_check( int event_type, void *data ) {
    nebstruct_host_check_data * hostdata;
    char *raw_command=NULL;
    char *processed_command=NULL;
    struct timeval start_time;
    host * hst;

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

    /* shouldn't happen - internal Nagios error */
    if ( hostdata == 0 ) {
        gm_log( GM_LOG_ERROR, "Host handler received NULL host data structure.\n" );
        return NEB_OK;
    }

    /* get objects and set target function */
    hst = find_host( hostdata->host_name );
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
    /* clear check options - we don't want old check options retained */
    hst->check_options = CHECK_OPTION_NONE;

    /* adjust host check attempt */
    adjust_host_check_attempt_3x(hst,TRUE);

    /* grab the host macro variables */
    clear_volatile_macros();
    grab_host_macros(hst);

    /* get the raw command line */
    get_raw_command_line(hst->check_command_ptr,hst->host_check_command,&raw_command,0);
    if(raw_command==NULL){
        gm_log( GM_LOG_ERROR, "Raw check command for host '%s' was NULL - aborting.\n",hst->name );
        return NEBERROR_CALLBACKCANCEL;
    }

    /* process any macros contained in the argument */
    process_macros(raw_command,&processed_command,0);
    if(processed_command==NULL){
        gm_log( GM_LOG_ERROR, "Processed check command for host '%s' was NULL - aborting.\n",hst->name);
        return NEBERROR_CALLBACKCANCEL;
    }

    /* get the command start time */
    gettimeofday(&start_time,NULL);

    /* increment number of host checks that are currently running */
    currently_running_host_checks++;

    /* set the execution flag */
    hst->is_executing=TRUE;

    gm_log( GM_LOG_TRACE, "cmd_line: %s\n", processed_command );

    temp_buffer[0]='\x0';
    snprintf( temp_buffer,sizeof( temp_buffer )-1,"type=host\nresult_queue=%s\nhost_name=%s\nstart_time=%i.%i\ntimeout=%d\ncommand_line=%s\n\n\n",
              mod_gm_opt->result_queue,
              hst->name,
              ( int )start_time.tv_sec,
              ( int )start_time.tv_usec,
              host_check_timeout,
              processed_command
            );
    temp_buffer[sizeof( temp_buffer )-1]='\x0';

    if(add_job_to_queue( &client,
                         mod_gm_opt->server_list,
                         target_queue,
                         hst->name,
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

    /* tell nagios to not execute */
    gm_log( GM_LOG_TRACE, "handle_host_check() finished successfully -> %d\n", NEBERROR_CALLBACKOVERRIDE );
    return NEBERROR_CALLBACKOVERRIDE;
}


/* handle service check events */
static int handle_svc_check( int event_type, void *data ) {
    host * hst   = NULL;
    service * svc = NULL;
    char *raw_command=NULL;
    char *processed_command=NULL;
    struct timeval start_time;
    nebstruct_service_check_data * svcdata;
    int prio = GM_JOB_PRIO_LOW;

    gm_log( GM_LOG_TRACE, "handle_svc_check(%i, data)\n", event_type );
    svcdata = ( nebstruct_service_check_data * )data;

    if ( event_type != NEBCALLBACK_SERVICE_CHECK_DATA )
        return NEB_OK;

    /* ignore non-initiate service checks */
    if ( svcdata->type != NEBTYPE_SERVICECHECK_ASYNC_PRECHECK )
        return NEB_OK;

    /* shouldn't happen - internal Nagios error */
    if ( svcdata == 0 ) {
        gm_log( GM_LOG_ERROR, "Service handler received NULL service data structure.\n" );
        return NEBERROR_CALLBACKCANCEL;
    }

    /* get objects and set target function */
    svc = find_service( svcdata->host_name, svcdata->service_description );

    /* find the host associated with this service */
    if((hst=svc->host_ptr)==NULL)
        return NEBERROR_CALLBACKCANCEL;
    set_target_queue( hst, svc );

    /* local check? */
    if(!strcmp( target_queue, "" )) {
        gm_log( GM_LOG_DEBUG, "passing by local servicecheck: %s - %s\n", svcdata->host_name, svcdata->service_description);
        return NEB_OK;
    }

    /* as we have to intercept service checks so early
     * (we cannot cancel checks otherwise)
     * we have to do some service check logic here
     * taken from checks.c:
     */
    /* clear check options - we don't want old check options retained */
    svc->check_options=CHECK_OPTION_NONE;

    /* grab the host and service macro variables */
    clear_volatile_macros();
    grab_host_macros(hst);
    grab_service_macros(svc);

    /* get the raw command line */
    get_raw_command_line(svc->check_command_ptr,svc->service_check_command,&raw_command,0);
    if(raw_command==NULL){
        gm_log( GM_LOG_ERROR, "Raw check command for service '%s' on host '%s' was NULL - aborting.\n", svc->description, svc->host_name );
        return NEBERROR_CALLBACKCANCEL;
    }

    /* process any macros contained in the argument */
    process_macros(raw_command, &processed_command, 0);
    if(processed_command==NULL) {
        gm_log( GM_LOG_ERROR, "Processed check command for service '%s' on host '%s' was NULL - aborting.\n", svc->description, svc->host_name);
        my_free(raw_command);
        return NEBERROR_CALLBACKCANCEL;
    }

    /* get the command start time */
    gettimeofday(&start_time,NULL);

    /* increment number of service checks that are currently running... */
    currently_running_service_checks++;

    /* set the execution flag */
    svc->is_executing=TRUE;

    gm_log( GM_LOG_DEBUG, "received job for queue %s: %s - %s\n", target_queue, svcdata->host_name, svcdata->service_description );
    gm_log( GM_LOG_TRACE, "cmd_line: %s\n", processed_command );

    temp_buffer[0]='\x0';
    snprintf( temp_buffer,sizeof( temp_buffer )-1,"type=service\nresult_queue=%s\nhost_name=%s\nservice_description=%s\nstart_time=%i.%i\ntimeout=%d\ncommand_line=%s\n\n\n",
              mod_gm_opt->result_queue,
              svcdata->host_name,
              svcdata->service_description,
              ( int )start_time.tv_sec,
              ( int )start_time.tv_usec,
              service_check_timeout,
              processed_command
            );
    temp_buffer[sizeof( temp_buffer )-1]='\x0';

    uniq[0]='\x0';
    snprintf( uniq,sizeof( temp_buffer )-1,"%s-%s", svcdata->host_name, svcdata->service_description);

    /* execute forced checks with high prio as they are propably user requested */
    if(check_result_info.check_options & CHECK_OPTION_FORCE_EXECUTION)
        prio = GM_JOB_PRIO_HIGH;

    if(add_job_to_queue( &client,
                         mod_gm_opt->server_list,
                         target_queue,
                         uniq,
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

    /* tell nagios to not execute */
    return NEBERROR_CALLBACKOVERRIDE;
}


/* parse the module arguments */
static int read_arguments( const char *args_orig ) {
    int verify;
    int errors = 0;
    char *ptr;
    char *args   = strdup(args_orig);
    char *args_c = args;
    while ( (ptr = strsep( &args, " " )) != NULL ) {
        if(parse_args_line(mod_gm_opt, ptr, 0) != GM_OK) {
            errors++;
            break;
        }
    }

    verify = verify_options(mod_gm_opt);

    if(mod_gm_opt->debug_level >= GM_LOG_DEBUG) {
        dumpconfig(mod_gm_opt, GM_NEB_MODE);
    }

    /* read keyfile */
    if(mod_gm_opt->keyfile != NULL && read_keyfile(mod_gm_opt) != GM_OK) {
        errors++;
    }

    free(args_c);

    if(errors > 0) {
        return(GM_ERROR);
    }

    return(verify);
}


/* verify our option */
static int verify_options(mod_gm_opt_t *opt) {
    /* did we get any server? */
    if(opt->server_num == 0) {
        gm_log( GM_LOG_ERROR, "please specify at least one server\n" );
        return(GM_ERROR);
    }

    if ( mod_gm_opt->result_queue == NULL )
        mod_gm_opt->result_queue = GM_DEFAULT_RESULT_QUEUE;

    /* nothing set by hand -> defaults */
    if( opt->set_queues_by_hand == 0 ) {
        gm_log( GM_LOG_DEBUG, "starting client with default queues\n" );
        opt->hosts    = GM_ENABLED;
        opt->services = GM_ENABLED;
        opt->events   = GM_ENABLED;
    }

    if(   opt->servicegroups_num == 0
       && opt->hostgroups_num    == 0
       && opt->exports_count     == 0
       && opt->hosts             == GM_DISABLED
       && opt->services          == GM_DISABLED
       && opt->events            == GM_DISABLED
       && opt->perfdata          == GM_DISABLED
      ) {
        gm_log( GM_LOG_ERROR, "loading Mod-Gearman NEB module without any queues is useless\n" );
        return(GM_ERROR);
    }

    /* do we need a result thread? */
    if(   opt->servicegroups_num == 0
       && opt->hostgroups_num    == 0
       && opt->hosts    == GM_DISABLED
       && opt->services == GM_DISABLED
      ) {
        gm_log( GM_LOG_DEBUG, "disabled unused result threads\n" );
        mod_gm_opt->result_workers = 0;
    }

    return(GM_OK);
}


/* return the prefered target function for our worker */
static void set_target_queue( host *hst, service *svc ) {
    int x=0;

    /* empty our target */
    target_queue[0] = '\x0';

    /* look for matching local servicegroups */
    if ( svc ) {
        while ( mod_gm_opt->local_servicegroups_list[x] != NULL ) {
            servicegroup * temp_servicegroup = find_servicegroup( mod_gm_opt->local_servicegroups_list[x] );
            if ( is_service_member_of_servicegroup( temp_servicegroup,svc )==TRUE ) {
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
        if ( is_host_member_of_hostgroup( temp_hostgroup,hst )==TRUE ) {
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
            if ( is_service_member_of_servicegroup( temp_servicegroup,svc )==TRUE ) {
                gm_log( GM_LOG_TRACE, "service is member of servicegroup: %s\n", mod_gm_opt->servicegroups_list[x] );
                snprintf( target_queue, sizeof(target_queue)-1, "servicegroup_%s", mod_gm_opt->servicegroups_list[x] );
                target_queue[sizeof( target_queue )-1]='\x0';
                return;
            }
            x++;
        }
    }

    /* look for matching hostgroups */
    x = 0;
    while ( mod_gm_opt->hostgroups_list[x] != NULL ) {
        hostgroup * temp_hostgroup = find_hostgroup( mod_gm_opt->hostgroups_list[x] );
        if ( is_host_member_of_hostgroup( temp_hostgroup,hst )==TRUE ) {
            gm_log( GM_LOG_TRACE, "server is member of hostgroup: %s\n", mod_gm_opt->hostgroups_list[x] );
            snprintf( target_queue, sizeof(target_queue)-1, "hostgroup_%s", mod_gm_opt->hostgroups_list[x] );
            target_queue[sizeof( target_queue )-1]='\x0';
            return;
        }
        x++;
    }

    if ( svc ) {
        /* pass into the general service queue */
        if ( mod_gm_opt->services == GM_ENABLED && svc ) {
            snprintf( target_queue, sizeof(target_queue)-1, "service" );
            target_queue[sizeof( target_queue )-1]='\x0';
            return;
        }
    }
    else {
        /* pass into the general host queue */
        if ( mod_gm_opt->hosts == GM_ENABLED ) {
            snprintf( target_queue, sizeof(target_queue)-1, "host" );
            target_queue[sizeof( target_queue )-1]='\x0';
            return;
        }
    }

    return;
}


/* start our threads */
static void start_threads(void) {
    if ( !result_threads_running ) {
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

                hst = find_host(hostchkdata->host_name);
                if(hst->process_performance_data == 0) {
                    gm_log( GM_LOG_TRACE, "handle_perfdata() process_performance_data disabled for: %s\n", hst->name );
                    break;
                }

                uniq[0]='\x0';
                snprintf( uniq,sizeof( temp_buffer )-1,"%s", hostchkdata->host_name);

                temp_buffer[0]='\x0';
                snprintf( temp_buffer,sizeof( temp_buffer )-1,
                            "DATATYPE::HOSTPERFDATA\t"
                            "TIMET::%d\t"
                            "HOSTNAME::%s\t"
                            "HOSTPERFDATA::%s\t"
                            "HOSTCHECKCOMMAND::%s!%s\t"
                            "HOSTSTATE::%d\t"
                            "HOSTSTATETYPE::%d\n",
                            (int)hostchkdata->timestamp.tv_sec,
                            hostchkdata->host_name, hostchkdata->perf_data,
                            hostchkdata->command_name, hostchkdata->command_args,
                            hostchkdata->state, hostchkdata->state_type);
                temp_buffer[sizeof( temp_buffer )-1]='\x0';
                has_perfdata = TRUE;
            }
            break;

        case NEBCALLBACK_SERVICE_CHECK_DATA:
            /* an aggregated status data dump just started or ended */
            if ((srvchkdata = (nebstruct_service_check_data *) data)) {

                if(srvchkdata->type != NEBTYPE_SERVICECHECK_PROCESSED || srvchkdata->perf_data == NULL) {
                    break;
                }

                /* find the nagios service object for this service */
                svc = find_service(srvchkdata->host_name, srvchkdata->service_description);
                if(svc->process_performance_data == 0) {
                    gm_log( GM_LOG_TRACE, "handle_perfdata() process_performance_data disabled for: %s - %s\n", svc->host_name, svc->description );
                    break;
                }

                uniq[0]='\x0';
                snprintf( uniq,sizeof( temp_buffer )-1,"%s-%s", srvchkdata->host_name, srvchkdata->service_description);

                temp_buffer[0]='\x0';
                snprintf( temp_buffer,sizeof( temp_buffer )-1,
                            "DATATYPE::SERVICEPERFDATA\t"
                            "TIMET::%d\t"
                            "HOSTNAME::%s\t"
                            "SERVICEDESC::%s\t"
                            "SERVICEPERFDATA::%s\t"
                            "SERVICECHECKCOMMAND::%s\t"
                            "SERVICESTATE::%d\t"
                            "SERVICESTATETYPE::%d\n\n\n",
                            (int)srvchkdata->timestamp.tv_sec,
                            srvchkdata->host_name, srvchkdata->service_description,
                            srvchkdata->perf_data, svc->service_check_command,
                            srvchkdata->state, srvchkdata->state_type);
                temp_buffer[sizeof( temp_buffer )-1]='\x0';
                has_perfdata = TRUE;
            }
            break;

        default:
            break;
    }

    if(has_perfdata == TRUE) {
        /* add our job onto the queue */
        if(add_job_to_queue( &client,
                             mod_gm_opt->server_list,
                             GM_PERFDATA_QUEUE,
                             (mod_gm_opt->perfdata_mode == GM_PERFDATA_OVERWRITE ? uniq : NULL),
                             temp_buffer,
                             GM_JOB_PRIO_NORMAL,
                             GM_DEFAULT_JOB_RETRIES,
                             mod_gm_opt->transportmode,
                             TRUE
                            ) == GM_OK) {
            gm_log( GM_LOG_TRACE, "handle_perfdata() finished successfully\n" );
        }
        else {
            gm_log( GM_LOG_TRACE, "handle_perfdata() finished unsuccessfully\n" );
        }
    }

    return 0;
}


/* handle generic exports */
int handle_export(int event_type, void *data) {
    int i;
    char * buffer;
    char * type;
    temp_buffer[0]          = '\x0';
    int debug_level_orig    = mod_gm_opt->debug_level;
    mod_gm_opt->debug_level = -1;
    int return_code         = 0;
    nebstruct_log_data          * nld;
    nebstruct_process_data      * npd;
    nebstruct_timed_event_data  * nted;

    /* what type of event/data do we have? */
    switch (event_type) {
        case NEBCALLBACK_RESERVED0:                         /*  0 */
            break;
        case NEBCALLBACK_RESERVED1:                         /*  1 */
            break;
        case NEBCALLBACK_RESERVED2:                         /*  2 */
            break;
        case NEBCALLBACK_RESERVED3:                         /*  3 */
            break;
        case NEBCALLBACK_RESERVED4:                         /*  4 */
            break;
        case NEBCALLBACK_RAW_DATA:                          /*  5 */
            break;
        case NEBCALLBACK_NEB_DATA:                          /*  6 */
            break;
        case NEBCALLBACK_PROCESS_DATA:                      /*  7 */
            npd = (nebstruct_process_data *)data;
            snprintf( temp_buffer,sizeof( temp_buffer )-1, "{\"event_type\":\"%s\",\"type\":\"%d\",\"flags\":%d,\"attr\":%d,\"timestamp\":%d.%d}",
                    "NEBCALLBACK_PROCESS_DATA",
                    npd->type,
                    npd->flags,
                    npd->attr,
                    (int)npd->timestamp.tv_sec, (int)npd->timestamp.tv_usec
                    );
            break;
        case NEBCALLBACK_TIMED_EVENT_DATA:                  /*  8 */
            nted = (nebstruct_timed_event_data *)data;
            snprintf( temp_buffer,sizeof( temp_buffer )-1, "{\"event_type\":\"%s\",\"type\":\"%d\",\"flags\":%d,\"attr\":%d,\"timestamp\":%d.%d,\"recurring\":%d,\"run_time\":%d}",
                    "NEBCALLBACK_TIMED_EVENT_DATA",
                    nted->type,
                    nted->flags,
                    nted->attr,
                    (int)nted->timestamp.tv_sec, (int)nted->timestamp.tv_usec,
                    nted->recurring,
                    (int)nted->run_time
                    );
            break;
        case NEBCALLBACK_LOG_DATA:                          /*  9 */
            nld    = (nebstruct_log_data *)data;
            buffer = escapestring(nld->data);
            type   = neb_type2str(nld->type);
            snprintf( temp_buffer,sizeof( temp_buffer )-1, "{\"event_type\":\"%s\",\"type\":\"%s\",\"flags\":%d,\"attr\":%d,\"timestamp\":%d.%d,\"entry_time\":%d,\"data_type\":%d,\"data\":\"%s\"}",
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
            gm_log( GM_LOG_ERROR, "handle_export() unknown export type: %d\n", event_type );
            mod_gm_opt->debug_level = debug_level_orig;
            return 0;
    }

    temp_buffer[sizeof( temp_buffer )-1]='\x0';
    if(temp_buffer[0] != '\x0') {

        for(i=0;i<mod_gm_opt->exports[event_type]->elem_number;i++) {
            return_code = mod_gm_opt->exports[event_type]->return_code[i];
            add_job_to_queue( &client,
                              mod_gm_opt->server_list,
                              mod_gm_opt->exports[event_type]->name[i], /* queue name */
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
