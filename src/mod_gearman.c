/******************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

/* include header */
#include "utils.h"
#include "result_thread.h"
#include "mod_gearman.h"
#include "logger.h"
#include "gearman.h"

/* specify event broker API version (required) */
NEB_API_VERSION( CURRENT_NEB_API_VERSION );
extern int currently_running_host_checks;

/* global variables */
void *gearman_module_handle=NULL;
gearman_client_st client;

char *gearman_hostgroups_list[GM_LISTSIZE];
char *gearman_servicegroups_list[GM_LISTSIZE];
char *gearman_local_hostgroups_list[GM_LISTSIZE];
char *gearman_local_servicegroups_list[GM_LISTSIZE];

int gearman_opt_services;
int gearman_opt_hosts;
int gearman_opt_events;
int gearman_opt_perfdata;

int gearman_threads_running = 0;
pthread_t result_thr[GM_LISTSIZE];
char target_worker[GM_BUFFERSIZE];
char temp_buffer[GM_BUFFERSIZE];
char uniq[GM_BUFFERSIZE];

static void  register_neb_callbacks(void);
static void  read_arguments( const char * );
static int   handle_host_check( int,void * );
static int   handle_svc_check( int,void * );
static int   handle_eventhandler( int,void * );
static int   handle_perfdata(int e, void *);
static void  set_target_worker( host *, service * );
static int   handle_process_events( int, void * );
static void  start_threads(void);


// this function gets initally called when loading the module
int nebmodule_init( int flags, char *args, nebmodule *handle ) {

    // save our handle
    gearman_module_handle=handle;

    // set some info
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_TITLE,   "mod_gearman" );
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_AUTHOR,  "Sven Nierlein" );
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_TITLE,   "Copyright (c) 2010 Sven Nierlein" );
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_VERSION, GM_VERSION );
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_LICENSE, "GPL v3" );
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_DESC,    "distribute host/service checks and eventhandler via gearman" );

    logger( GM_LOG_INFO, "Version %s\n", GM_VERSION );

    // parse arguments
    read_arguments( args );
    logger( GM_LOG_DEBUG, "args: %s\n", args );
    logger( GM_LOG_TRACE, "nebmodule_init(%i, %i)\n", flags );
    logger( GM_LOG_DEBUG, "running on libgearman %s\n", gearman_version() );

    if((float)atof(gearman_version()) < (float)GM_MIN_LIB_GEARMAN_VERSION) {
        logger( GM_LOG_ERROR, "minimum version of libgearman is %.2f, yours is %.2f\n", (float)GM_MIN_LIB_GEARMAN_VERSION, (float)atof(gearman_version()) );
        return GM_ERROR;
    }

    // create client
    if ( create_gearman_client( gearman_opt_server, &client ) != GM_OK ) {
        logger( GM_LOG_ERROR, "cannot start client\n" );
        return GM_ERROR;
    }

    // register callback for process event where everything else starts
    neb_register_callback( NEBCALLBACK_PROCESS_DATA, gearman_module_handle, 0, handle_process_events );

    logger( GM_LOG_DEBUG, "finished initializing\n" );

    return GM_OK;
}


/* register eventhandler callback */
static void register_neb_callbacks(void) {

    // only if we have hostgroups defined or general hosts GM_ENABLED
    if ( gearman_hostgroups_list[0] != NULL || gearman_opt_hosts == GM_ENABLED )
        neb_register_callback( NEBCALLBACK_HOST_CHECK_DATA,    gearman_module_handle, 0, handle_host_check );

    // only if we have groups defined or general services GM_ENABLED
    if ( gearman_servicegroups_list[0] != NULL || gearman_hostgroups_list[0] != NULL || gearman_opt_services == GM_ENABLED )
        neb_register_callback( NEBCALLBACK_SERVICE_CHECK_DATA, gearman_module_handle, 0, handle_svc_check );

    if ( gearman_opt_events == GM_ENABLED )
        neb_register_callback( NEBCALLBACK_EVENT_HANDLER_DATA, gearman_module_handle, 0, handle_eventhandler );

    if ( gearman_opt_perfdata == GM_ENABLED ) {
        neb_register_callback( NEBCALLBACK_HOST_CHECK_DATA, gearman_module_handle, 0, handle_perfdata );
        neb_register_callback( NEBCALLBACK_SERVICE_CHECK_DATA, gearman_module_handle, 0, handle_perfdata );
    }

    logger( GM_LOG_DEBUG, "registered neb callbacks\n" );
}


/* deregister all events */
int nebmodule_deinit( int flags, int reason ) {

    logger( GM_LOG_TRACE, "nebmodule_deinit(%i, %i)\n", flags, reason );

    // should be removed already, but just for the case it wasn't
    neb_deregister_callback( NEBCALLBACK_PROCESS_DATA, gearman_module_handle );

    // only if we have hostgroups defined or general hosts GM_ENABLED
    if ( gearman_hostgroups_list[0] != NULL || gearman_opt_hosts == GM_ENABLED )
        neb_deregister_callback( NEBCALLBACK_HOST_CHECK_DATA, gearman_module_handle );

    // only if we have groups defined or general services GM_ENABLED
    if ( gearman_servicegroups_list[0] != NULL || gearman_hostgroups_list[0] != NULL || gearman_opt_services == GM_ENABLED )
        neb_deregister_callback( NEBCALLBACK_SERVICE_CHECK_DATA, gearman_module_handle );

    if ( gearman_opt_events == GM_ENABLED )
        neb_deregister_callback( NEBCALLBACK_EVENT_HANDLER_DATA, gearman_module_handle );

    if ( gearman_opt_perfdata == GM_ENABLED ) {
        neb_deregister_callback( NEBCALLBACK_HOST_CHECK_DATA, gearman_module_handle );
        neb_deregister_callback( NEBCALLBACK_SERVICE_CHECK_DATA, gearman_module_handle );
    }

    logger( GM_LOG_DEBUG, "deregistered callbacks\n" );

    // stop result threads
    int x;
    for(x = 0; x < gearman_opt_result_workers; x++) {
        pthread_cancel(result_thr[x]);
        pthread_join(result_thr[x], NULL);
    }

    return GM_OK;
}


/* handle process events */
static int handle_process_events( int event_type, void *data ) {

    logger( GM_LOG_TRACE, "handle_process_events(%i, data)\n", event_type );

    struct nebstruct_process_struct *ps = ( struct nebstruct_process_struct * )data;
    if ( ps->type == NEBTYPE_PROCESS_EVENTLOOPSTART ) {
        register_neb_callbacks();
        start_threads();

        neb_deregister_callback( NEBCALLBACK_PROCESS_DATA, gearman_module_handle );

        // verify names of supplied groups
        // this cannot be done befor nagios has finished reading his config
        // verify local servicegroups names
        int x=0;
        while ( gearman_local_servicegroups_list[x] != NULL ) {
            servicegroup * temp_servicegroup = find_servicegroup( gearman_local_servicegroups_list[x] );
            if( temp_servicegroup == NULL ) {
                logger( GM_LOG_INFO, "Warning: servicegroup '%s' does not exist, possible typo?\n", gearman_local_servicegroups_list[x] );
            }
            x++;
        }

        // verify local hostgroup names
        x = 0;
        while ( gearman_local_hostgroups_list[x] != NULL ) {
            hostgroup * temp_hostgroup = find_hostgroup( gearman_local_hostgroups_list[x] );
            if( temp_hostgroup == NULL ) {
                logger( GM_LOG_INFO, "Warning: hostgroup '%s' does not exist, possible typo?\n", gearman_local_hostgroups_list[x] );
            }
            x++;
        }

        // verify servicegroups names
        x = 0;
        while ( gearman_servicegroups_list[x] != NULL ) {
            servicegroup * temp_servicegroup = find_servicegroup( gearman_servicegroups_list[x] );
            if( temp_servicegroup == NULL ) {
                logger( GM_LOG_INFO, "Warning: servicegroup '%s' does not exist, possible typo?\n", gearman_servicegroups_list[x] );
            }
            x++;
        }

        // verify hostgroup names
        x = 0;
        while ( gearman_hostgroups_list[x] != NULL ) {
            hostgroup * temp_hostgroup = find_hostgroup( gearman_hostgroups_list[x] );
            if( temp_hostgroup == NULL ) {
                logger( GM_LOG_INFO, "Warning: hostgroup '%s' does not exist, possible typo?\n", gearman_hostgroups_list[x] );
            }
            x++;
        }
    }

    return GM_OK;
}


/* handle eventhandler events */
static int handle_eventhandler( int event_type, void *data ) {

    logger( GM_LOG_DEBUG, "got eventhandler event\n" );
    logger( GM_LOG_TRACE, "handle_eventhandler(%i, data)\n", event_type );

    if ( event_type != NEBTYPE_EVENTHANDLER_START )
        return GM_OK;

    nebstruct_event_handler_data * ds = ( nebstruct_event_handler_data * )data;

    logger( GM_LOG_TRACE, "got eventhandler event: %s\n", ds->command_line );

    temp_buffer[0]='\x0';
    snprintf( temp_buffer,sizeof( temp_buffer )-1,"type=eventhandler\ncommand_line=%s\n",ds->command_line );
    temp_buffer[sizeof( temp_buffer )-1]='\x0';

    if(add_job_to_queue( &client,
                         "eventhandler",
                         NULL,
                         temp_buffer,
                         GM_JOB_PRIO_NORMAL,
                         GM_DEFAULT_JOB_RETRIES
                        ) == GM_OK) {
        logger( GM_LOG_TRACE, "handle_eventhandler() finished successfully\n" );
    }

    // tell nagios to not execute
    return NEBERROR_CALLBACKOVERRIDE;
}


/* handle host check events */
static int handle_host_check( int event_type, void *data ) {

    logger( GM_LOG_TRACE, "handle_host_check(%i, data)\n", event_type );

    nebstruct_host_check_data * hostdata = ( nebstruct_host_check_data * )data;

    logger( GM_LOG_DEBUG, "---------------\nhost Job -> %i vs %i, %i vs %i\n", event_type, NEBCALLBACK_HOST_CHECK_DATA, hostdata->type, NEBTYPE_HOSTCHECK_ASYNC_PRECHECK );

    if ( event_type != NEBCALLBACK_HOST_CHECK_DATA )
        return GM_OK;

    // ignore non-initiate service checks
    if ( hostdata->type != NEBTYPE_HOSTCHECK_ASYNC_PRECHECK )
        return GM_OK;

    // shouldn't happen - internal Nagios error
    if ( hostdata == 0 ) {
        logger( GM_LOG_ERROR, "Host handler received NULL host data structure.\n" );
        return GM_ERROR;
    }

    host * hst = find_host( hostdata->host_name );
    set_target_worker( hst, NULL );

    // local check?
    if(!strcmp( target_worker, "" )) {
        logger( GM_LOG_DEBUG, "passing by local hostcheck: %s\n", hostdata->host_name );
        return GM_OK;
    }

    logger( GM_LOG_DEBUG, "Received Job for queue %s: %s\n", target_worker, hostdata->host_name );

    // as we have to intercept host checks much earlier than service checks
    // we have to do some host check logic here
    // taken from checks.c:
    char *raw_command=NULL;
    char *processed_command=NULL;
    //double old_latency=0.0;
    struct timeval start_time;

    // clear check options - we don't want old check options retained
    hst->check_options = CHECK_OPTION_NONE;

    // adjust host check attempt
    adjust_host_check_attempt_3x(hst,TRUE);

    // grab the host macro variables
    clear_volatile_macros();
    grab_host_macros(hst);

    // get the raw command line
    get_raw_command_line(hst->check_command_ptr,hst->host_check_command,&raw_command,0);
    if(raw_command==NULL){
        logger( GM_LOG_ERROR, "Raw check command for host '%s' was NULL - aborting.\n",hst->name );
        return GM_ERROR;
    }

    // process any macros contained in the argument
    process_macros(raw_command,&processed_command,0);
    if(processed_command==NULL){
        logger( GM_LOG_ERROR, "Processed check command for host '%s' was NULL - aborting.\n",hst->name);
        return GM_ERROR;
    }

    // get the command start time
    gettimeofday(&start_time,NULL);

    // increment number of host checks that are currently running
    currently_running_host_checks++;

    // set the execution flag
    hst->is_executing=TRUE;

    logger( GM_LOG_TRACE, "cmd_line: %s\n", processed_command );

    //extern check_result check_result_info;
    temp_buffer[0]='\x0';
    snprintf( temp_buffer,sizeof( temp_buffer )-1,"type=host\nresult_queue=%s\nhost_name=%s\nstart_time=%i.%i\ntimeout=%d\ncommand_line=%s\n",
              gearman_opt_result_queue,
              hst->name,
              ( int )start_time.tv_sec,
              ( int )start_time.tv_usec,
              hostdata->timeout,
              processed_command
            );
    temp_buffer[sizeof( temp_buffer )-1]='\x0';

    if(add_job_to_queue( &client,
                         target_worker,
                         hst->name,
                         temp_buffer,
                         GM_JOB_PRIO_NORMAL,
                         GM_DEFAULT_JOB_RETRIES
                        ) == GM_OK) {
        logger( GM_LOG_TRACE, "handle_host_check() finished successfully\n" );
    }

    // clean up
    my_free(raw_command);
    my_free(processed_command);

    // tell nagios to not execute
    return NEBERROR_CALLBACKOVERRIDE;
}


/* handle service check events */
static int handle_svc_check( int event_type, void *data ) {

    logger( GM_LOG_TRACE, "handle_svc_check(%i, data)\n", event_type );
    nebstruct_service_check_data * svcdata = ( nebstruct_service_check_data * )data;

    if ( event_type != NEBCALLBACK_SERVICE_CHECK_DATA )
        return GM_OK;

    // ignore non-initiate service checks
    if ( svcdata->type != NEBTYPE_SERVICECHECK_INITIATE )
        return GM_OK;

    // shouldn't happen - internal Nagios error
    if ( svcdata == 0 ) {
        logger( GM_LOG_ERROR, "Service handler received NULL service data structure.\n" );
        return GM_ERROR;
    }

    service * svc = find_service( svcdata->host_name, svcdata->service_description );
    host * host   = find_host( svcdata->host_name );
    set_target_worker( host, svc );

    // local check?
    if(!strcmp( target_worker, "" )) {
        logger( GM_LOG_DEBUG, "passing by local servicecheck: %s - %s\n", svcdata->host_name, svcdata->service_description);
        return GM_OK;
    }

    logger( GM_LOG_DEBUG, "Received Job for queue %s: %s - %s\n", target_worker, svcdata->host_name, svcdata->service_description );
    logger( GM_LOG_TRACE, "cmd_line: %s\n", svcdata->command_line );

    extern check_result check_result_info;
    temp_buffer[0]='\x0';
    snprintf( temp_buffer,sizeof( temp_buffer )-1,"type=service\nresult_queue=%s\nhost_name=%s\nservice_description=%s\nstart_time=%i.%i\ntimeout=%d\ncheck_options=%i\nscheduled_check=%i\nreschedule_check=%i\nlatency=%f\ncommand_line=%s\n",
              gearman_opt_result_queue,
              svcdata->host_name,
              svcdata->service_description,
              ( int )svcdata->start_time.tv_sec,
              ( int )svcdata->start_time.tv_usec,
              svcdata->timeout,
              check_result_info.check_options,
              check_result_info.scheduled_check,
              check_result_info.reschedule_check,
              check_result_info.latency,
              svcdata->command_line
            );
    temp_buffer[sizeof( temp_buffer )-1]='\x0';

    uniq[0]='\x0';
    snprintf( uniq,sizeof( temp_buffer )-1,"%s-%s", svcdata->host_name, svcdata->service_description);

    // execute forced checks with high prio as they are propably user requested
    int prio = GM_JOB_PRIO_LOW;
    if(check_result_info.check_options & CHECK_OPTION_FORCE_EXECUTION)
        prio = GM_JOB_PRIO_HIGH;

    if(add_job_to_queue( &client,
                         target_worker,
                         uniq,
                         temp_buffer,
                         prio,
                         GM_DEFAULT_JOB_RETRIES
                        ) == GM_OK) {
        logger( GM_LOG_TRACE, "handle_svc_check() finished successfully\n" );
    }

    // tell nagios to not execute
    return NEBERROR_CALLBACKOVERRIDE;
}


/* parse the module arguments */
static void read_arguments( const char *args_orig ) {

    gearman_opt_timeout        = 0;
    gearman_opt_result_workers = 1;
    gearman_opt_result_queue   = NULL;
    gearman_opt_events         = GM_DISABLED;
    gearman_opt_services       = GM_DISABLED;
    gearman_opt_hosts          = GM_DISABLED;
    gearman_opt_perfdata       = GM_DISABLED;

    // no arguments given
    if ( !args_orig )
        return;

    char *ptr;
    int  srv_ptr         = 0;
    int  srvgrp_ptr      = 0;
    int  hostgrp_ptr     = 0;
    int  loc_srvgrp_ptr  = 0;
    int  loc_hostgrp_ptr = 0;
    char * args      = strdup( args_orig );
    while ( (ptr = strsep( &args, " " )) != NULL ) {
        char *key   = str_token( &ptr, '=' );
        char *value = str_token( &ptr, 0 );

        if ( key == NULL )
            continue;

        if ( value == NULL )
            continue;

        if ( !strcmp( key, "debug" ) || !strcmp( key, "--debug" ) ) {
            gearman_opt_debug_level = atoi( value );
            logger( GM_LOG_DEBUG, "Setting debug level to %d\n", gearman_opt_debug_level );
        }
        else if ( !strcmp( key, "timeout" ) || !strcmp( key, "--timeout" ) ) {
            gearman_opt_timeout = atoi( value );
            logger( GM_LOG_DEBUG, "Setting timeout to %d\n", gearman_opt_timeout );
        }
        else if ( !strcmp( key, "result_workers" ) || !strcmp( key, "--result_workers" ) ) {
            gearman_opt_result_workers = atoi( value );
            if(gearman_opt_result_workers > GM_LISTSIZE) { gearman_opt_result_workers = GM_LISTSIZE; }
            if(gearman_opt_result_workers <= 0)       { gearman_opt_result_workers = 1; }
            logger( GM_LOG_DEBUG, "Setting result_workers to %d\n", gearman_opt_result_workers );
        }
        else if ( !strcmp( key, "result_queue" ) || !strcmp( key, "--result_queue" ) ) {
            gearman_opt_result_queue = value;
            logger( GM_LOG_DEBUG, "set result queue to '%s'\n", gearman_opt_result_queue );
        }
        else if ( !strcmp( key, "server" ) || !strcmp( key, "--server" ) ) {
            char *servername;
            while ( (servername = strsep( &value, "," )) != NULL ) {
                if ( strcmp( servername, "" ) ) {
                    gearman_opt_server[srv_ptr] = servername;
                    srv_ptr++;
                }
            }
        }
        else if ( !strcmp( key, "eventhandler" ) || !strcmp( key, "--eventhandler" ) ) {
            if ( !strcmp( value, "yes" ) ) {
                gearman_opt_events = GM_ENABLED;
                logger( GM_LOG_DEBUG, "enabled handling of eventhandlers\n" );
            }
        }
        else if ( !strcmp( key, "perfdata" ) || !strcmp( key, "--perfdata" ) ) {
            if ( !strcmp( value, "yes" ) ) {
                gearman_opt_perfdata = GM_ENABLED;
                logger( GM_LOG_DEBUG, "enabled handling of perfdata\n" );
            }
        }
        else if ( !strcmp( key, "services" ) || !strcmp( key, "--services" ) ) {
            if ( !strcmp( value, "yes" ) ) {
                gearman_opt_services = GM_ENABLED;
                logger( GM_LOG_DEBUG, "enabled handling of service checks\n" );
            }
        }
        else if ( !strcmp( key, "hosts" ) || !strcmp( key, "--hosts" ) ) {
            if ( !strcmp( value, "yes" ) ) {
                gearman_opt_hosts = GM_ENABLED;
                logger( GM_LOG_DEBUG, "enabled handling of hostchecks\n" );
            }
        }
        else if ( !strcmp( key, "servicegroups" ) || !strcmp( key, "--servicegroups" ) ) {
            char *groupname;
            while ( (groupname = strsep( &value, "," )) != NULL ) {
                if ( strcmp( groupname, "" ) ) {
                    gearman_servicegroups_list[srvgrp_ptr] = groupname;
                    srvgrp_ptr++;
                    logger( GM_LOG_DEBUG, "added seperate worker for servicegroup: %s\n", groupname );
                }
            }
        }
        else if ( !strcmp( key, "hostgroups" ) || !strcmp( key, "--hostgroups" ) ) {
            char *groupname;
            while ( (groupname = strsep( &value, "," )) != NULL ) {
                if ( strcmp( groupname, "" ) ) {
                    gearman_hostgroups_list[hostgrp_ptr] = groupname;
                    hostgrp_ptr++;
                    logger( GM_LOG_DEBUG, "added seperate worker for hostgroup: %s\n", groupname );
                }
            }
        }
        else if ( !strcmp( key, "localhostgroups" ) || !strcmp( key, "--localhostgroups" ) ) {
            char *groupname;
            while ( (groupname = strsep( &value, "," )) != NULL ) {
                if ( strcmp( groupname, "" ) ) {
                    gearman_local_hostgroups_list[loc_hostgrp_ptr] = groupname;
                    loc_hostgrp_ptr++;
                    logger( GM_LOG_DEBUG, "added local hostgroup for: %s\n", groupname );
                }
            }
        }
        else if ( !strcmp( key, "localservicegroups" ) || !strcmp( key, "--localservicegroups" ) ) {
            char *groupname;
            while ( (groupname = strsep( &value, "," )) != NULL ) {
                if ( strcmp( groupname, "" ) ) {
                    gearman_local_servicegroups_list[loc_srvgrp_ptr] = groupname;
                    loc_srvgrp_ptr++;
                    logger( GM_LOG_DEBUG, "added local servicegroup for: %s\n", groupname );
                }
            }
        } else  {
            logger( GM_LOG_ERROR, "unknown option: key: %s value: %s\n", key, value );
        }
    }

    if ( gearman_opt_result_queue == NULL )
        gearman_opt_result_queue = "check_results";

    if ( gearman_opt_timeout == 0 )
        gearman_opt_timeout     = 3000;

    // did we get any server?
    if(srv_ptr == 0) {
        logger( GM_LOG_ERROR, "please specify at least one server\n" );
        return;
    }

    // do we need a result thread?
    if(srvgrp_ptr == 0 && hostgrp_ptr == 0 && gearman_opt_hosts == GM_DISABLED && gearman_opt_services == GM_DISABLED) {
        logger( GM_LOG_DEBUG, "disabled unused result threads\n" );
        gearman_opt_result_workers = 0;
    }

    return;
}


/* return the prefered target function for our worker */
static void set_target_worker( host *host, service *svc ) {

    // empty our target
    target_worker[0] = '\x0';

    // look for matching local servicegroups
    int x=0;
    if ( svc ) {
        while ( gearman_local_servicegroups_list[x] != NULL ) {
            servicegroup * temp_servicegroup = find_servicegroup( gearman_local_servicegroups_list[x] );
            if ( is_service_member_of_servicegroup( temp_servicegroup,svc )==TRUE ) {
                logger( GM_LOG_TRACE, "service is member of local servicegroup: %s\n", gearman_local_servicegroups_list[x] );
                return;
            }
            x++;
        }
    }

    // look for matching local hostgroups
    x = 0;
    while ( gearman_local_hostgroups_list[x] != NULL ) {
        hostgroup * temp_hostgroup = find_hostgroup( gearman_local_hostgroups_list[x] );
        if ( is_host_member_of_hostgroup( temp_hostgroup,host )==TRUE ) {
            logger( GM_LOG_TRACE, "server is member of local hostgroup: %s\n", gearman_local_hostgroups_list[x] );
            return;
        }
        x++;
    }

    // look for matching servicegroups
    x = 0;
    if ( svc ) {
        while ( gearman_servicegroups_list[x] != NULL ) {
            servicegroup * temp_servicegroup = find_servicegroup( gearman_servicegroups_list[x] );
            if ( is_service_member_of_servicegroup( temp_servicegroup,svc )==TRUE ) {
                logger( GM_LOG_TRACE, "service is member of servicegroup: %s\n", gearman_servicegroups_list[x] );
                snprintf( target_worker, sizeof(target_worker)-1, "servicegroup_%s", gearman_servicegroups_list[x] );
                target_worker[sizeof( target_worker )-1]='\x0';
                return;
            }
            x++;
        }
    }

    // look for matching hostgroups
    x = 0;
    while ( gearman_hostgroups_list[x] != NULL ) {
        hostgroup * temp_hostgroup = find_hostgroup( gearman_hostgroups_list[x] );
        if ( is_host_member_of_hostgroup( temp_hostgroup,host )==TRUE ) {
            logger( GM_LOG_TRACE, "server is member of hostgroup: %s\n", gearman_hostgroups_list[x] );
            snprintf( target_worker, sizeof(target_worker)-1, "hostgroup_%s", gearman_hostgroups_list[x] );
            target_worker[sizeof( target_worker )-1]='\x0';
            return;
        }
        x++;
    }

    if ( svc ) {
        // pass into the general service queue
        if ( gearman_opt_services == GM_ENABLED && svc ) {
            snprintf( target_worker, sizeof(target_worker)-1, "service" );
            target_worker[sizeof( target_worker )-1]='\x0';
            return;
        }
    }
    else {
        // pass into the general host queue
        if ( gearman_opt_hosts == GM_ENABLED ) {
            snprintf( target_worker, sizeof(target_worker)-1, "host" );
            target_worker[sizeof( target_worker )-1]='\x0';
            return;
        }
    }

    return;
}


/* start our threads */
static void start_threads(void) {
    if ( !gearman_threads_running ) {
        // create result worker
        int x;
        for(x = 0; x < gearman_opt_result_workers; x++) {
            gearman_threads_running++;
            worker_parm *p;
            p = (worker_parm *)malloc(sizeof(worker_parm));
            p->id = gearman_threads_running;
            pthread_create ( &result_thr[x], NULL, result_worker, (void *)p);
        }
    }
}


/* handle performance data */
int handle_perfdata(int event_type, void *data) {
    nebstruct_host_check_data *hostchkdata   = NULL;
    nebstruct_service_check_data *srvchkdata = NULL;

    host *host       = NULL;
    service *service = NULL;

    char perfdatafile_template[GM_BUFFERSIZE];
    int has_perfdata = FALSE;

    // what type of event/data do we have?
    switch (event_type) {

        case NEBCALLBACK_HOST_CHECK_DATA:
            // an aggregated status data dump just started or ended
            if ((hostchkdata = (nebstruct_host_check_data *) data)) {

                host = find_host(hostchkdata->host_name);

                if (hostchkdata->type == NEBTYPE_HOSTCHECK_PROCESSED
                    && hostchkdata->perf_data != NULL) {

                    snprintf(perfdatafile_template, sizeof(perfdatafile_template) - 1,
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

                    perfdatafile_template[sizeof(perfdatafile_template) - 1] = '\x0';
                    has_perfdata = TRUE;
                }
            }
            break;

        case NEBCALLBACK_SERVICE_CHECK_DATA:
            // an aggregated status data dump just started or ended
            if ((srvchkdata = (nebstruct_service_check_data *) data)) {

                if (srvchkdata->type == NEBTYPE_SERVICECHECK_PROCESSED
                    && srvchkdata->perf_data != NULL) {

                    // find the nagios service object for this service
                    service = find_service(srvchkdata->host_name, srvchkdata->service_description);

                    snprintf(perfdatafile_template, sizeof(perfdatafile_template) - 1,
                                "DATATYPE::SERVICEPERFDATA\t"
                                "TIMET::%d\t"
                                "HOSTNAME::%s\t"
                                "SERVICEDESC::%s\t"
                                "SERVICEPERFDATA::%s\t"
                                "SERVICECHECKCOMMAND::%s\t"
                                "SERVICESTATE::%d\t"
                                "SERVICESTATETYPE::%d\n",
                                (int)srvchkdata->timestamp.tv_sec,
                                srvchkdata->host_name, srvchkdata->service_description,
                                srvchkdata->perf_data, service->service_check_command,
                                srvchkdata->state, srvchkdata->state_type);
                    perfdatafile_template[sizeof(perfdatafile_template) - 1] = '\x0';
                    has_perfdata = TRUE;
                }
            }
            break;

        default:
            break;
    }

    if(has_perfdata == TRUE) {
        // add our job onto the queue
        if(add_job_to_queue( &client,
                             GM_PERFDATA_QUEUE,
                             NULL,
                             perfdatafile_template,
                             GM_JOB_PRIO_NORMAL,
                             GM_DEFAULT_JOB_RETRIES
                            ) == GM_OK) {
            logger( GM_LOG_TRACE, "handle_perfdata() finished successfully\n" );
        }
    }


    return 0;
}
