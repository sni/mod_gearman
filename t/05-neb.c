#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <ltdl.h>
#include <t/tap.h>

#include <config.h>

#ifdef USENAGIOS3
#include "nagios3/nagios.h"
#include "nagios3/nebmodules.h"
#include "nagios3/nebstructs.h"
#include "nagios3/nebcallbacks.h"
#include "nagios3/broker.h"
#define BROKER_MODULE "mod_gearman_nagios3.o"
#endif
#ifdef USENAGIOS4
#include "nagios4/nagios.h"
#include "nagios4/nebmodules.h"
#include "nagios4/nebstructs.h"
#include "nagios4/nebcallbacks.h"
#include "nagios4/broker.h"
#define BROKER_MODULE "mod_gearman_nagios4.o"
#endif
#ifdef USENAEMON
#include "naemon/naemon.h"
#include "naemon/nebmodules.h"
#include "naemon/nebstructs.h"
#include "naemon/nebcallbacks.h"
#include "naemon/broker.h"
#define BROKER_MODULE "mod_gearman_naemon.o"
#endif

int service_check_timeout;
int host_check_timeout;
int currently_running_service_checks;
int currently_running_host_checks;
#ifdef USENAGIOS3
int event_broker_options;
#endif
#if defined(USENAEMON) || defined(USENAGIOS4)
unsigned long event_broker_options;
#ifdef USENAEMON
timed_event *schedule_event(time_t delay, event_callback callback, void *user_data) { delay = delay; callback = callback; user_data = user_data; return(NULL); }
#endif
#endif
#if defined(USENAGIOS3) || defined(USENAGIOS4)
check_result *check_result_list;
check_result check_result_info;
#endif
int process_performance_data;

void check_neb(char * nebargs);
void check_neb(char * nebargs) {
    int (*initfunc)(int,char *,void *);
    int (*deinitfunc)(int,int);
    int *module_version_ptr=NULL;
    void* neb_handle;
    lt_ptr init_func;
    lt_ptr deinit_func;
    char *err;

    /* set some external variables */
    service_check_timeout            = 30;
    host_check_timeout               = 30;
    currently_running_service_checks = 0;
    currently_running_host_checks    = 0;
    event_broker_options             = 1048575; /* BROKER_EVERYTHING */
    check_result_list                = NULL;
    check_result_info.check_options  = 1;       /* CHECK_OPTION_FORCE_EXECUTION */
    process_performance_data         = 1;

    /* load neb module */
    neb_handle=(void *)dlopen("./"BROKER_MODULE, RTLD_LAZY|RTLD_GLOBAL);
    ok(neb_handle != NULL, "neb module loaded");
    err = dlerror(); if(err != NULL) { BAIL_OUT("cannot load module: %s\n", err ); }
    module_version_ptr=(int *)dlsym(neb_handle,"__neb_api_version");
    ok(module_version_ptr != NULL, "got module pointer %p", module_version_ptr);
    ok((*module_version_ptr) == CURRENT_NEB_API_VERSION, "got module api version %i", CURRENT_NEB_API_VERSION);

    /* init neb module */
    dlerror();
    init_func=(void *)dlsym(neb_handle,"nebmodule_init");
    ok(init_func != NULL, "located nebmodule_init()");
    err = dlerror(); if(err != NULL) { BAIL_OUT("cannot load module: %s\n", err ); }

    initfunc = init_func;
    int result=(*initfunc)(NEBMODULE_NORMAL_LOAD, nebargs, neb_handle);
    ok(result == 0, "run nebmodule_init() -> %d", result);

    /* deinit neb module */
    dlerror();
    deinit_func=(void *)dlsym(neb_handle,"nebmodule_deinit");
    ok(deinit_func != NULL, "located nebmodule_deinit()");
    err = dlerror(); if(err != NULL) { BAIL_OUT("cannot load module: %s\n", err ); }

    deinitfunc=deinit_func;
    result=(*deinitfunc)(NEBMODULE_FORCE_UNLOAD,NEBMODULE_NEB_SHUTDOWN);
    ok(result == 0, "run nebmodule_deinit() -> %d", result);
    err = dlerror(); if(err != NULL) { BAIL_OUT("cannot load module: %s\n", err ); }


    result=dlclose(neb_handle);
    ok(result == 0, "dlclose() -> %d", result);

    return;
}

/* core log wrapper */
void write_core_log(char *data);
void write_core_log(char *data) {
    printf("logger: %s", data);
    return;
}

/* fake some core functions */
int neb_set_module_info(void *handle, int type, char *data) { handle=handle; type=type; data=data; return 0; }
int neb_register_callback(int callback_type, void *mod_handle, int priority, int (*callback_func)(int,void *)) { callback_type=callback_type; mod_handle=mod_handle; priority=priority; callback_func=callback_func; return 0; }
int neb_deregister_callback(int callback_type, int (*callback_func)(int,void *)) { callback_type=callback_type; callback_func=callback_func; return 0; }


int main(void) {
    int i;

    plan(32);

    char * test_nebargs[] = {
        "encryption=no server=localhost",
        "key=test12345 server=localhost",
        "encryption=no server=localhost export=log_queue:1:NEBCALLBACK_LOG_DATA",
        "encryption=no server=localhost export=log_queue:1:NEBCALLBACK_LOG_DATA export=proc_queue:0:NEBCALLBACK_PROCESS_DATA",
    };

    int num = sizeof(test_nebargs) / sizeof(test_nebargs[0]);
    for(i=0;i<num;i++) {
        check_neb(test_nebargs[i]);
    }

    return exit_status();
}
