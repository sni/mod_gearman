#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <t/tap.h>

#include "nagios/nagios.h"
#include "nagios/nebmodules.h"
#include "nagios/nebstructs.h"
#include "nagios/nebcallbacks.h"
#include "nagios/broker.h"

int service_check_timeout;
int host_check_timeout;
int currently_running_service_checks;
int currently_running_host_checks;
int event_broker_options;
check_result *check_result_list;
check_result check_result_info;
int process_performance_data;

int main(void) {
    int i;

    plan(28);

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


void check_neb(char * nebargs);
void check_neb(char * nebargs) {
    int (*initfunc)(int,char *,void *);
    int (*deinitfunc)(int,int);
    int (*callfunc)(int,void *);
    int *module_version_ptr=NULL;
    void* neb_handle;
    lt_ptr init_func;
    lt_ptr deinit_func;
    lt_ptr call_func;

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
    neb_handle=(void *)dlopen("./mod_gearman.o",RTLD_LAZY|RTLD_GLOBAL);
    ok(neb_handle != NULL, "neb module loaded") || BAIL_OUT("cannot load module: %s\n", dlerror());
    module_version_ptr=(int *)dlsym(neb_handle,"__neb_api_version");
    ok((*module_version_ptr) == CURRENT_NEB_API_VERSION, "got module api version %i", CURRENT_NEB_API_VERSION);

    /* init neb module */
    init_func=(void *)dlsym(neb_handle,"nebmodule_init");
    ok(init_func != NULL, "located nebmodule_init()") || BAIL_OUT("cannot locate nebmodule_init() %s\n", dlerror());
    initfunc = init_func;
    int result=(*initfunc)(NEBMODULE_NORMAL_LOAD, nebargs, neb_handle);
    ok(result == 0, "run nebmodule_init() -> %d", result) || BAIL_OUT("cannot run nebmodule_init(), got %d\n", result);

    /* callback */
    //call_func=(void *)dlsym(neb_handle,"handle_process_events");
/*
    call_func=(void *)dlsym(neb_handle,"handle_eventhandler");
    ok(call_func != NULL, "located handle_process_events()") || BAIL_OUT("cannot locate handle_process_events() %s\n", dlerror());
    callfunc=call_func;
    nebstruct_process_data ps;
    ps.type = NEBTYPE_PROCESS_EVENTLOOPSTART;
    result=(*callfunc)(NEBCALLBACK_PROCESS_DATA,(void *)&ps);
    ok(result == 0, "run handle_process_events() -> %d", result) || BAIL_OUT("cannot run handle_process_events(), got %d\n", result);
*/

    /* deinit neb module */
    deinit_func=(void *)dlsym(neb_handle,"nebmodule_deinit");
    ok(deinit_func != NULL, "located nebmodule_deinit()") || BAIL_OUT("cannot locate nebmodule_deinit() %s\n", dlerror());
    deinitfunc=deinit_func;
    result=(*deinitfunc)(NEBMODULE_FORCE_UNLOAD,NEBMODULE_NEB_SHUTDOWN);
    ok(result == 0, "run nebmodule_deinit() -> %d", result) || BAIL_OUT("cannot run nebmodule_deinit(), got %d\n", result);


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
int neb_set_module_info(void *handle, int type, char *data) { return 0; }
int neb_register_callback(int callback_type, void *mod_handle, int priority, int (*callback_func)(int,void *)) { return 0; }
int neb_deregister_callback(int callback_type, int (*callback_func)(int,void *)) { return 0; }