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
NEB_API_VERSION(CURRENT_NEB_API_VERSION);

/* global variables */
void *gearman_module_handle=NULL;
gearman_client_st client;

char *gearman_hostgroups_list[LISTSIZE];
char *gearman_servicegroups_list[LISTSIZE];
static int gearman_opt_timeout     = 3000;

int gearman_opt_services;
int gearman_opt_hosts;
int gearman_opt_events;

char * result_queue_name = "check_result_queue";
int gearman_threads_running = 0;
pthread_t result_thr;

// this function gets initally called when loading the module
int nebmodule_init(int flags, char *args, nebmodule *handle) {

    // save our handle
    gearman_module_handle=handle;

    // set some info
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_TITLE,   "mod_gearman");
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_AUTHOR,  "Sven Nierlein");
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_TITLE,   "Copyright (c) 2010 Sven Nierlein");
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_VERSION, "0.1");
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_LICENSE, "GPL v3");
    neb_set_module_info( gearman_module_handle, NEBMODULE_MODINFO_DESC,    "distribute host/service checks and eventhandler via gearman");

    logger(GM_INFO, "Version %s - Copyright (c) 2010 Sven Nierlein\n", MOD_GEARMAN_VERSION);

    // parse arguments
    read_arguments(args);
    logger(GM_DEBUG, "args: %s\n", args);

    if(create_gearman_client() != OK) {
        logger(GM_ERROR, "could not create gearman client\n");
        return ERROR;
    }

    // register callback for process event where everything else starts
    neb_register_callback(NEBCALLBACK_PROCESS_DATA, gearman_module_handle, 0, handle_process_events);

    logger(GM_DEBUG, "finished initializing\n");

    return OK;
}


/* register eventhandler callback */
static void register_neb_callbacks() {

    if(gearman_opt_hosts == ENABLED)
        neb_register_callback(NEBCALLBACK_HOST_CHECK_DATA,    gearman_module_handle, 0, handle_host_check);

    if(gearman_opt_services == ENABLED)
        neb_register_callback(NEBCALLBACK_SERVICE_CHECK_DATA, gearman_module_handle, 0, handle_svc_check);

    if(gearman_opt_events == ENABLED)
        neb_register_callback(NEBCALLBACK_EVENT_HANDLER_DATA, gearman_module_handle, 0, handle_eventhandler);

    logger(GM_DEBUG, "registered neb callbacks\n");
}


/* deregister all events */
static int nebmodule_deinit(int flags, int reason){

    neb_deregister_callback(NEBCALLBACK_PROCESS_DATA, gearman_module_handle);

    if(gearman_opt_hosts == ENABLED)
        neb_deregister_callback(NEBCALLBACK_HOST_CHECK_DATA, gearman_module_handle);

    if(gearman_opt_services == ENABLED)
        neb_deregister_callback(NEBCALLBACK_SERVICE_CHECK_DATA, gearman_module_handle);

    if(gearman_opt_events == ENABLED)
        neb_deregister_callback(NEBCALLBACK_EVENT_HANDLER_DATA, gearman_module_handle);

    logger(GM_DEBUG, "deregistered callbacks\n");
    return OK;
}


/* handle process events */
static int handle_process_events(int event_type, void *data) {
    struct nebstruct_process_struct *ps = (struct nebstruct_process_struct *)data;
    if (ps->type == NEBTYPE_PROCESS_EVENTLOOPSTART) {
        register_neb_callbacks();
        start_threads();
    }
    return OK;
}


/* handle eventhandler events */
static int handle_eventhandler(int event_type, void *data) {

    logger(GM_DEBUG, "got eventhandler event\n");

    if (event_type != NEBTYPE_EVENTHANDLER_START)
        return OK;

    nebstruct_event_handler_data * ds = (nebstruct_event_handler_data *)data;

    logger(GM_TRACE, "got eventhandler event: %s\n", ds->command_line);

    char temp_buffer[BUFFERSIZE];
    snprintf(temp_buffer,sizeof(temp_buffer)-1,"{\"command_line\":\"%s\"}\n",ds->command_line);
    temp_buffer[sizeof(temp_buffer)-1]='\x0';

    gearman_task_st  *task;
    gearman_return_t ret;
    gearman_client_add_task_background(&client, task, NULL, "service", NULL, (void *)temp_buffer, (size_t)strlen(temp_buffer), &ret);
    gearman_client_run_tasks(&client);

    // tell nagios to not execute
    return NEBERROR_CALLBACKOVERRIDE;
}


/* handle host check events */
static int handle_host_check(int event_type, void *data){

    nebstruct_host_check_data * hostdata = (nebstruct_host_check_data *)data;

    logger(GM_DEBUG, "---------------\nhost Job -> %i vs %i, %i vs %i\n", event_type, NEBCALLBACK_HOST_CHECK_DATA, hostdata->type, NEBTYPE_HOSTCHECK_INITIATE);

    if(event_type != NEBCALLBACK_HOST_CHECK_DATA)
        return OK;

    // ignore non-initiate service checks
    if(hostdata->type != NEBTYPE_HOSTCHECK_INITIATE)
        return OK;

    // shouldn't happen - internal Nagios error
    if(hostdata == 0) {
      logger(GM_ERROR, "Host handler received NULL host data structure.\n");
      return ERROR;
    }

    if( gearman_client_errno(&client) != GEARMAN_SUCCESS ) {
        logger(GM_ERROR, "client error %d: %s\n", gearman_client_errno(&client), gearman_client_error(&client));
        return ERROR;
    }

    host * host         = find_host(hostdata->host_name);
    char *target_worker = get_target_worker(host, NULL);

    logger(GM_DEBUG, "Received Job for queue %s: %s\n", target_worker, hostdata->host_name);
    logger(GM_TRACE, "cmd_line: %s\n", hostdata->command_line);

    extern check_result check_result_info;
    char temp_buffer[BUFFERSIZE];
    snprintf(temp_buffer,sizeof(temp_buffer)-1,"{\"result_queue\":\"%s\",\"host_name\":\"%s\",\"start_time\":\"%i.%i\",\"timeout\":\"%d\",\"check_options\":\"%i\",\"scheduled_check\":\"%i\",\"reschedule_check\":\"%i\",\"command_line\":\"%s\"}\n",
                result_queue_name,
                hostdata->host_name,
                (int)hostdata->start_time.tv_sec,
                (int)hostdata->start_time.tv_usec,
                hostdata->timeout,
                check_result_info.check_options,
                check_result_info.scheduled_check,
                check_result_info.reschedule_check,
                hostdata->command_line
            );
    temp_buffer[sizeof(temp_buffer)-1]='\x0';

    gearman_task_st  *task;
    gearman_return_t ret;
    gearman_client_add_task_background(&client, task, NULL, target_worker, NULL, (void *)temp_buffer, (size_t)strlen(temp_buffer), &ret);
    gearman_client_run_tasks(&client);

    // tell nagios to not execute
    return NEBERROR_CALLBACKOVERRIDE;
}


/* handle service check events */
static int handle_svc_check(int event_type, void *data){

    nebstruct_service_check_data * svcdata = (nebstruct_service_check_data *)data;

    if(event_type != NEBCALLBACK_SERVICE_CHECK_DATA)
        return OK;

    // ignore non-initiate service checks
    if(svcdata->type != NEBTYPE_SERVICECHECK_INITIATE)
        return OK;

    // shouldn't happen - internal Nagios error
    if(svcdata == 0) {
      logger(GM_ERROR, "Service handler received NULL service data structure.\n");
      return ERROR;
    }

    service * svc = find_service(svcdata->host_name, svcdata->service_description);
    host * host   = find_host(svcdata->host_name);
    char *target_worker = get_target_worker(host, svc);

    logger(GM_DEBUG, "Received Job for queue %s: %s - %s\n", target_worker, svcdata->host_name, svcdata->service_description);
    logger(GM_TRACE, "cmd_line: %s\n", svcdata->command_line);

    extern check_result check_result_info;
    char temp_buffer[BUFFERSIZE];
    snprintf(temp_buffer,sizeof(temp_buffer)-1,"{\"result_queue\":\"%s\",\"host_name\":\"%s\",\"service_description\":\"%s\",\"start_time\":\"%i.%i\",\"timeout\":\"%d\",\"check_options\":\"%i\",\"scheduled_check\":\"%i\",\"reschedule_check\":\"%i\",\"command_line\":\"%s\"}\n",
                result_queue_name,
                svcdata->host_name,
                svcdata->service_description,
                (int)svcdata->start_time.tv_sec,
                (int)svcdata->start_time.tv_usec,
                svcdata->timeout,
                check_result_info.check_options,
                check_result_info.scheduled_check,
                check_result_info.reschedule_check,
                svcdata->command_line
            );
    temp_buffer[sizeof(temp_buffer)-1]='\x0';

    gearman_task_st *task;
    gearman_return_t ret;
    gearman_client_add_task_background(&client, task, NULL, target_worker, NULL, (void *)temp_buffer, (size_t)strlen(temp_buffer), &ret);
    gearman_client_run_tasks(&client);

    // tell nagios to not execute
    return NEBERROR_CALLBACKOVERRIDE;
}


/* parse the module arguments */
static void read_arguments(const char *args_orig) {

    // no arguments given
    if (!args_orig)
        return;

    char *ptr;
    int  srv_ptr     = 0;
    int  srvgrp_ptr  = 0;
    int  hostgrp_ptr = 0;
    char * args = strdup(args_orig);
    while(ptr = strsep(&args, " ")) {
        char *key   = str_token(&ptr, '=');
        char *value = str_token(&ptr, 0);

        if(!strcmp(key, "debug")) {
            gearman_opt_debug_level = atoi(value);
            logger(GM_DEBUG, "Setting debug level to %d\n", gearman_opt_debug_level);
        }
        else if(!strcmp(key, "timeout")) {
            gearman_opt_timeout = atoi(value);
            logger(GM_DEBUG, "Setting timeout to %d\n", gearman_opt_timeout);
        }
        else if(!strcmp(key, "server")) {
            gearman_opt_server[srv_ptr] = value;
            srv_ptr++;
        }
        else if(!strcmp(key, "eventhandler")) {
            if(!strcmp(value, "yes")) {
                gearman_opt_events = ENABLED;
                logger(GM_DEBUG, "enabled handling of eventhandlers\n");
            }
        }
        else if(!strcmp(key, "services")) {
            if(!strcmp(value, "yes")) {
                gearman_opt_services = ENABLED;
                logger(GM_DEBUG, "enabled handling of service checks\n");
            }
        }
        else if(!strcmp(key, "hosts")) {
            if(!strcmp(value, "yes")) {
                gearman_opt_hosts = ENABLED;
                logger(GM_DEBUG, "enabled handling of hostchecks\n");
            }
        }
        else if(!strcmp(key, "servicegroups")) {
            char *groupname;
            while(groupname = strsep(&value, ",")) {
                if(strcmp(groupname, "")) {
                    gearman_servicegroups_list[srvgrp_ptr] = groupname;
                    srvgrp_ptr++;
                    logger(GM_DEBUG, "added seperate worker for servicegroup: %s\n", groupname);
                }
            }
        }
        else if(!strcmp(key, "hostgroups")) {
            char *groupname;
            while(groupname = strsep(&value, ",")) {
                if(strcmp(groupname, "")) {
                    gearman_hostgroups_list[hostgrp_ptr] = groupname;
                    hostgrp_ptr++;
                    logger(GM_DEBUG, "added seperate worker for hostgroup: %s\n", groupname);
                    groupname = strtok(value, ",");
                }
            }
        }
        else if(!strcmp(key, "")) {
            logger(GM_ERROR, "unknown option: key: %s value: %s\n", key, value);
        }
    }

    return;
}


/* create the gearman client */
static int create_gearman_client() {
    gearman_return_t ret;
    if(gearman_client_create(&client) == NULL) {
        logger(GM_ERROR, "Memory allocation failure on client creation\n");
        return ERROR;
    }

    int x = 0;
    while(gearman_opt_server[x] != NULL) {
        char * server  = strdup(gearman_opt_server[x]);
        char * host    = str_token(&server, ':');
        in_port_t port = (in_port_t) atoi(str_token(&server, 0));
        ret=gearman_client_add_server(&client, host, port);
        if (ret != GEARMAN_SUCCESS) {
            logger(GM_ERROR, "client error: %s\n", gearman_client_error(&client));
            return ERROR;
        }
        logger(GM_DEBUG, "added gearman server %s:%i\n", host, port);
        x++;
    }

    gearman_client_set_timeout(&client, gearman_opt_timeout);

    return OK;
}


/* return the prefered target function for our worker */
static char *get_target_worker(host *host, service *svc) {

    static char target_worker[BUFFERSIZE];
    sprintf(target_worker, "");

    // look for matching servicegroups
    int x=0;
    if(svc) {
        while(!strcmp(target_worker,"") && gearman_servicegroups_list[x] != NULL) {
            servicegroup * temp_servicegroup = find_servicegroup(gearman_servicegroups_list[x]);
            if(is_service_member_of_servicegroup(temp_servicegroup,svc)==TRUE){
                logger(GM_TRACE, "service is member of servicegroup: %s\n", gearman_servicegroups_list[x]);
                sprintf(target_worker, "servicegroup_%s", gearman_servicegroups_list[x]);
            }
            x++;
        }
    }

    // look for matching hostgroups
    x = 0;
    while(!strcmp(target_worker,"") && gearman_hostgroups_list[x] != NULL) {
        hostgroup * temp_hostgroup = find_hostgroup(gearman_hostgroups_list[x]);
        if(is_host_member_of_hostgroup(temp_hostgroup,host)==TRUE){
            logger(GM_TRACE, "server is member of hostgroup: %s\n", gearman_hostgroups_list[x]);
            sprintf(target_worker, "hostgroup_%s", gearman_hostgroups_list[x]);
        }
        x++;
    }

    if(!strcmp(target_worker,"")) {
        if(svc) {
            sprintf(target_worker, "service");
        } else {
            sprintf(target_worker, "host");
        }
    }

    return target_worker;
}


/* start our threads */
static void start_threads() {
    if (!gearman_threads_running) {
        // create result worker
        result_worker_arg args;
        args.timeout = gearman_opt_timeout;
        args.result_queue_name = strdup(result_queue_name);
        int x = 0;
        while(gearman_opt_server[x] != NULL) {
            args.server[x]  = strdup(gearman_opt_server[x]);
            x++;
        }
        args.server[x] = NULL;
        pthread_create (&result_thr, NULL, result_worker, &args);
        gearman_threads_running = 1;
    }
}
