/*****************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

/* include header */
#include "utils.h"
#include "worker.h"
#include "mod_gearman.h"
#include "logger.h"

/* specify event broker API version (required) */
NEB_API_VERSION( CURRENT_NEB_API_VERSION );
extern int currently_running_host_checks;

/* global variables */
void *gearman_module_handle=NULL;
gearman_client_st client;

char *gearman_hostgroups_list[LISTSIZE];
char *gearman_servicegroups_list[LISTSIZE];

int gearman_opt_services;
int gearman_opt_hosts;
int gearman_opt_events;

int gearman_threads_running = 0;
pthread_t result_thr;
char target_worker[BUFFERSIZE];
char temp_buffer[BUFFERSIZE];
char uniq[BUFFERSIZE];

static void  register_neb_callbacks(void);
static void  read_arguments( const char * );
static int   handle_host_check( int,void * );
static int   handle_svc_check( int,void * );
static int   handle_eventhandler( int,void * );
static int   create_gearman_client(void);
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
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_VERSION, "0.1" );
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_LICENSE, "GPL v3" );
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_DESC,    "distribute host/service checks and eventhandler via gearman" );

    logger( GM_INFO, "Version %s\n", MOD_GEARMAN_VERSION );

    // parse arguments
    read_arguments( args );
    logger( GM_DEBUG, "args: %s\n", args );

    logger( GM_TRACE, "nebmodule_init(%i, %i)\n", flags );

    // create gearman client
    logger( GM_DEBUG, "running on libgearman %s\n", gearman_version() );
    if ( create_gearman_client() != OK ) {
        logger( GM_ERROR, "could not create gearman client\n" );
        return ERROR;
    }

    // register callback for process event where everything else starts
    neb_register_callback( NEBCALLBACK_PROCESS_DATA, gearman_module_handle, 0, handle_process_events );

    logger( GM_DEBUG, "finished initializing\n" );

    return OK;
}


/* register eventhandler callback */
static void register_neb_callbacks(void) {

    if ( gearman_opt_hosts == ENABLED )
        neb_register_callback( NEBCALLBACK_HOST_CHECK_DATA,    gearman_module_handle, 0, handle_host_check );

    if ( gearman_opt_services == ENABLED )
        neb_register_callback( NEBCALLBACK_SERVICE_CHECK_DATA, gearman_module_handle, 0, handle_svc_check );

    if ( gearman_opt_events == ENABLED )
        neb_register_callback( NEBCALLBACK_EVENT_HANDLER_DATA, gearman_module_handle, 0, handle_eventhandler );

    logger( GM_DEBUG, "registered neb callbacks\n" );
}


/* deregister all events */
int nebmodule_deinit( int flags, int reason ) {

    logger( GM_TRACE, "nebmodule_deinit(%i, %i)\n", flags, reason );

    neb_deregister_callback( NEBCALLBACK_PROCESS_DATA, gearman_module_handle );

    if ( gearman_opt_hosts == ENABLED )
        neb_deregister_callback( NEBCALLBACK_HOST_CHECK_DATA, gearman_module_handle );

    if ( gearman_opt_services == ENABLED )
        neb_deregister_callback( NEBCALLBACK_SERVICE_CHECK_DATA, gearman_module_handle );

    if ( gearman_opt_events == ENABLED )
        neb_deregister_callback( NEBCALLBACK_EVENT_HANDLER_DATA, gearman_module_handle );

    logger( GM_DEBUG, "deregistered callbacks\n" );

    // stop result thread
    pthread_cancel (result_thr);
    pthread_join (result_thr, NULL);

    return OK;
}


/* handle process events */
static int handle_process_events( int event_type, void *data ) {

    logger( GM_TRACE, "handle_process_events(%i, data)\n", event_type );

    struct nebstruct_process_struct *ps = ( struct nebstruct_process_struct * )data;
    if ( ps->type == NEBTYPE_PROCESS_EVENTLOOPSTART ) {
        register_neb_callbacks();
        start_threads();

        neb_deregister_callback( NEBCALLBACK_PROCESS_DATA, gearman_module_handle );
    }
    return OK;
}


/* handle eventhandler events */
static int handle_eventhandler( int event_type, void *data ) {

    logger( GM_DEBUG, "got eventhandler event\n" );
    logger( GM_TRACE, "handle_eventhandler(%i, data)\n", event_type );

    if ( event_type != NEBTYPE_EVENTHANDLER_START )
        return OK;

    nebstruct_event_handler_data * ds = ( nebstruct_event_handler_data * )data;

    logger( GM_TRACE, "got eventhandler event: %s\n", ds->command_line );

    temp_buffer[0]='\x0';
    snprintf( temp_buffer,sizeof( temp_buffer )-1,"type=eventhandler\ncommand_line=%s\n",ds->command_line );
    temp_buffer[sizeof( temp_buffer )-1]='\x0';

    gearman_task_st  *task = NULL;
    gearman_return_t ret;
    gearman_client_add_task_background( &client, task, NULL, "service", NULL, ( void * )temp_buffer, ( size_t )strlen( temp_buffer ), &ret );
    gearman_client_run_tasks( &client );
    if(gearman_client_error(&client) != NULL && strcmp(gearman_client_error(&client), "") != 0) { // errno is somehow empty, use error instead
        gearman_client_free(&client);
        create_gearman_client();
    }

    // tell nagios to not execute
    return NEBERROR_CALLBACKOVERRIDE;
}


/* handle host check events */
static int handle_host_check( int event_type, void *data ) {

    logger( GM_TRACE, "handle_host_check(%i, data)\n", event_type );

    nebstruct_host_check_data * hostdata = ( nebstruct_host_check_data * )data;

    logger( GM_DEBUG, "---------------\nhost Job -> %i vs %i, %i vs %i\n", event_type, NEBCALLBACK_HOST_CHECK_DATA, hostdata->type, NEBTYPE_HOSTCHECK_ASYNC_PRECHECK );

    if ( event_type != NEBCALLBACK_HOST_CHECK_DATA )
        return OK;

    // ignore non-initiate service checks
    if ( hostdata->type != NEBTYPE_HOSTCHECK_ASYNC_PRECHECK )
        return OK;

    // shouldn't happen - internal Nagios error
    if ( hostdata == 0 ) {
        logger( GM_ERROR, "Host handler received NULL host data structure.\n" );
        return ERROR;
    }

    if ( gearman_client_errno( &client ) != GEARMAN_SUCCESS ) {
        logger( GM_ERROR, "client error %d: %s\n", gearman_client_errno( &client ), gearman_client_error( &client ) );
        return ERROR;
    }

    host * hst = find_host( hostdata->host_name );
    set_target_worker( hst, NULL );

    logger( GM_DEBUG, "Received Job for queue %s: %s\n", target_worker, hostdata->host_name );

    // as we have to intercept host checks much earlier than service checks
    // we have to do some host check logic here
    // taken from checks.c:
    char *raw_command=NULL;
    char *processed_command=NULL;
    //double old_latency=0.0;
    struct timeval start_time;

    /* clear check options - we don't want old check options retained */
    /* only clear options if this was a scheduled check - on demand check options shouldn't affect retained info */
    //if(scheduled_check==TRUE)
        hst->check_options = CHECK_OPTION_NONE;

    // adjust host check attempt
    adjust_host_check_attempt_3x(hst,TRUE);

    /* set latency (temporarily) for macros and event broker */
    //old_latency=hst->latency;
    //hst->latency=latency;

    /* grab the host macro variables */
    clear_volatile_macros();
    grab_host_macros(hst);

    /* get the raw command line */
    get_raw_command_line(hst->check_command_ptr,hst->host_check_command,&raw_command,0);
    if(raw_command==NULL){
        logger( GM_ERROR, "Raw check command for host '%s' was NULL - aborting.\n",hst->name );
        return ERROR;
    }

    /* process any macros contained in the argument */
    process_macros(raw_command,&processed_command,0);
    if(processed_command==NULL){
        logger( GM_ERROR, "Processed check command for host '%s' was NULL - aborting.\n",hst->name);
        return ERROR;
    }

    /* get the command start time */
    gettimeofday(&start_time,NULL);

    /* set check time for on-demand checks, so they're not incorrectly detected as being orphaned - Luke Ross 5/16/08 */
    /* NOTE: 06/23/08 EG not sure if there will be side effects to this or not.... */
    //if(scheduled_check==FALSE)
    //    hst->next_check=start_time.tv_sec;

    /* increment number of host checks that are currently running... */
    currently_running_host_checks++;

    /* set the execution flag */
    hst->is_executing=TRUE;

    logger( GM_TRACE, "cmd_line: %s\n", processed_command );

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

    gearman_task_st  *task = NULL;
    gearman_return_t ret;
    gearman_client_add_task_background( &client, task, NULL, target_worker, hst->name, ( void * )temp_buffer, ( size_t )strlen( temp_buffer ), &ret );
    gearman_client_run_tasks( &client );
    if(gearman_client_error(&client) != NULL && strcmp(gearman_client_error(&client), "") != 0) { // errno is somehow empty, use error instead
        gearman_client_free(&client);
        create_gearman_client();
    }

    // clean up
    my_free(raw_command);
    my_free(processed_command);

    // tell nagios to not execute
    return NEBERROR_CALLBACKOVERRIDE;
}


/* handle service check events */
static int handle_svc_check( int event_type, void *data ) {

    logger( GM_TRACE, "handle_svc_check(%i, data)\n", event_type );
    nebstruct_service_check_data * svcdata = ( nebstruct_service_check_data * )data;

    if ( event_type != NEBCALLBACK_SERVICE_CHECK_DATA )
        return OK;

    // ignore non-initiate service checks
    if ( svcdata->type != NEBTYPE_SERVICECHECK_INITIATE )
        return OK;

    // shouldn't happen - internal Nagios error
    if ( svcdata == 0 ) {
        logger( GM_ERROR, "Service handler received NULL service data structure.\n" );
        return ERROR;
    }

    service * svc = find_service( svcdata->host_name, svcdata->service_description );
    host * host   = find_host( svcdata->host_name );
    set_target_worker( host, svc );

    logger( GM_DEBUG, "Received Job for queue %s: %s - %s\n", target_worker, svcdata->host_name, svcdata->service_description );
    logger( GM_TRACE, "cmd_line: %s\n", svcdata->command_line );

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
    gearman_task_st *task = NULL;
    gearman_return_t ret;
    gearman_client_add_task_low_background( &client, task, NULL, target_worker, uniq, ( void * )temp_buffer, ( size_t )strlen( temp_buffer ), &ret );
    gearman_client_run_tasks( &client );
    if(gearman_client_error(&client) != NULL && strcmp(gearman_client_error(&client), "") != 0) { // errno is somehow empty, use error instead
        gearman_client_free(&client);
        create_gearman_client();
    }

    // tell nagios to not execute
    return NEBERROR_CALLBACKOVERRIDE;
}


/* parse the module arguments */
static void read_arguments( const char *args_orig ) {

    gearman_opt_timeout      = 0;
    gearman_opt_result_queue = NULL;
    gearman_opt_events       = DISABLED;
    gearman_opt_services     = DISABLED;
    gearman_opt_hosts        = DISABLED;

    // no arguments given
    if ( !args_orig )
        return;

    char *ptr;
    int  srv_ptr     = 0;
    int  srvgrp_ptr  = 0;
    int  hostgrp_ptr = 0;
    char * args = strdup( args_orig );
    while ( (ptr = strsep( &args, " " )) != NULL ) {
        char *key   = str_token( &ptr, '=' );
        char *value = str_token( &ptr, 0 );

        if ( !strcmp( key, "debug" ) ) {
            gearman_opt_debug_level = atoi( value );
            logger( GM_DEBUG, "Setting debug level to %d\n", gearman_opt_debug_level );
        } else if ( !strcmp( key, "timeout" ) ) {
            gearman_opt_timeout = atoi( value );
            logger( GM_DEBUG, "Setting timeout to %d\n", gearman_opt_timeout );
        } else if ( !strcmp( key, "result_queue" ) ) {
            gearman_opt_result_queue = value;
            logger( GM_DEBUG, "set result queue to '%s'\n", gearman_opt_result_queue );
        } else if ( !strcmp( key, "server" ) ) {
            char *servername;
            while ( (servername = strsep( &value, "," )) != NULL ) {
                if ( strcmp( servername, "" ) ) {
                    gearman_opt_server[srv_ptr] = servername;
                    srv_ptr++;
                }
            }
        } else if ( !strcmp( key, "eventhandler" ) ) {
            if ( !strcmp( value, "yes" ) ) {
                gearman_opt_events = ENABLED;
                logger( GM_DEBUG, "enabled handling of eventhandlers\n" );
            }
        } else if ( !strcmp( key, "services" ) ) {
            if ( !strcmp( value, "yes" ) ) {
                gearman_opt_services = ENABLED;
                logger( GM_DEBUG, "enabled handling of service checks\n" );
            }
        } else if ( !strcmp( key, "hosts" ) ) {
            if ( !strcmp( value, "yes" ) ) {
                gearman_opt_hosts = ENABLED;
                logger( GM_DEBUG, "enabled handling of hostchecks\n" );
            }
        } else if ( !strcmp( key, "servicegroups" ) ) {
            char *groupname;
            while ( (groupname = strsep( &value, "," )) != NULL ) {
                if ( strcmp( groupname, "" ) ) {
                    gearman_servicegroups_list[srvgrp_ptr] = groupname;
                    srvgrp_ptr++;
                    logger( GM_DEBUG, "added seperate worker for servicegroup: %s\n", groupname );
                }
            }
        } else if ( !strcmp( key, "hostgroups" ) ) {
            char *groupname;
            while ( (groupname = strsep( &value, "," )) != NULL ) {
                if ( strcmp( groupname, "" ) ) {
                    gearman_hostgroups_list[hostgrp_ptr] = groupname;
                    hostgrp_ptr++;
                    logger( GM_DEBUG, "added seperate worker for hostgroup: %s\n", groupname );
                    groupname = strtok( value, "," );
                }
            }
        } else  {
            logger( GM_ERROR, "unknown option: key: %s value: %s\n", key, value );
        }
    }
    //free(args);

    if ( gearman_opt_result_queue == NULL )
        gearman_opt_result_queue = "check_results";

    if ( gearman_opt_timeout == 0 )
        gearman_opt_timeout     = 3000;

    return;
}


/* create the gearman client */
static int create_gearman_client(void) {
    gearman_return_t ret;
    if ( gearman_client_create( &client ) == NULL ) {
        logger( GM_ERROR, "Memory allocation failure on client creation\n" );
        return ERROR;
    }

    int x = 0;
    while ( gearman_opt_server[x] != NULL ) {
        char * server   = strdup( gearman_opt_server[x] );
        char * server_c = server;
        char * host     = str_token( &server, ':' );
        in_port_t port  = ( in_port_t ) atoi( str_token( &server, 0 ) );
        ret = gearman_client_add_server( &client, host, port );
        if ( ret != GEARMAN_SUCCESS ) {
            logger( GM_ERROR, "client error: %s\n", gearman_client_error( &client ) );
            free(server_c);
            return ERROR;
        }
        logger( GM_DEBUG, "client added gearman server %s:%i\n", host, port );
        free(server_c);
        x++;
    }

    gearman_client_set_timeout( &client, gearman_opt_timeout );

    return OK;
}


/* return the prefered target function for our worker */
static void set_target_worker( host *host, service *svc ) {

    // empty our target
    target_worker[0] = '\x0';

    // look for matching servicegroups
    int x=0;
    if ( svc ) {
        while ( !strcmp( target_worker,"" ) && gearman_servicegroups_list[x] != NULL ) {
            servicegroup * temp_servicegroup = find_servicegroup( gearman_servicegroups_list[x] );
            if ( is_service_member_of_servicegroup( temp_servicegroup,svc )==TRUE ) {
                logger( GM_TRACE, "service is member of servicegroup: %s\n", gearman_servicegroups_list[x] );
                snprintf( target_worker, sizeof(target_worker)-1, "servicegroup_%s", gearman_servicegroups_list[x] );
            }
            x++;
        }
    }

    // look for matching hostgroups
    x = 0;
    while ( !strcmp( target_worker,"" ) && gearman_hostgroups_list[x] != NULL ) {
        hostgroup * temp_hostgroup = find_hostgroup( gearman_hostgroups_list[x] );
        if ( is_host_member_of_hostgroup( temp_hostgroup,host )==TRUE ) {
            logger( GM_TRACE, "server is member of hostgroup: %s\n", gearman_hostgroups_list[x] );
            snprintf( target_worker, sizeof(target_worker)-1, "hostgroup_%s", gearman_hostgroups_list[x] );
        }
        x++;
    }

    if ( !strcmp( target_worker,"" ) ) {
        if ( svc ) {
            snprintf( target_worker, sizeof(target_worker)-1, "service" );
        } else {
            snprintf( target_worker, sizeof(target_worker)-1, "host" );
        }
    }

    target_worker[sizeof( target_worker )-1]='\x0';

    return;
}


/* start our threads */
static void start_threads(void) {
    if ( !gearman_threads_running ) {
        // create result worker
        gearman_threads_running++;
        worker_parm *p;
        p = (worker_parm *)malloc(sizeof(worker_parm));
        p->id = gearman_threads_running;
        pthread_create ( &result_thr, NULL, result_worker, (void *)p);
    }
}
