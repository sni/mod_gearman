#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <ltdl.h>
#include <t/tap.h>

#include <config.h>

#include "naemon/naemon.h"
#include "naemon/nebmodules.h"
#include "naemon/nebstructs.h"
#include "naemon/nebcallbacks.h"
#include "naemon/broker.h"
#define BROKER_MODULE "mod_gearman_naemon.o"

#include "common.h"
#include <worker_dummy_functions.c>

#include <libgearman/gearman.h>

mod_gm_opt_t *mod_gm_opt;
char hostname[GM_SMALLBUFSIZE];
gearman_client_st *current_client;
gearman_client_st *current_client_dup;

int service_check_timeout;
int host_check_timeout;
int currently_running_service_checks;
int currently_running_host_checks;
unsigned long event_broker_options;
timed_event *schedule_event(__attribute__((unused)) time_t delay, __attribute__((unused)) event_callback callback, __attribute__((unused)) void *user_data) { return(NULL); }
int process_performance_data;
int log_notifications;

#pragma GCC diagnostic push    //Save actual diagnostics state
#pragma GCC diagnostic ignored "-Wpedantic"    //Disable pedantic

void check_neb(char * nebargs);
void check_neb(char * nebargs) {
    int *module_version_ptr=NULL;
    void* neb_handle;
    int (*init_func)(int,char *, void*);
    int (*deinit_func)(int,int);
    char *err;

    /* set some external variables */
    service_check_timeout            = 30;
    host_check_timeout               = 30;
    currently_running_service_checks = 0;
    currently_running_host_checks    = 0;
    event_broker_options             = 1048575; /* BROKER_EVERYTHING */
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
    init_func = (int(*)(int,char *, void*))dlsym(neb_handle,"nebmodule_init");
    ok(init_func != NULL, "located nebmodule_init()");
    err = dlerror(); if(err != NULL) { BAIL_OUT("cannot load module: %s\n", err ); }

    int result=init_func(NEBMODULE_NORMAL_LOAD, nebargs, neb_handle);
    ok(result == 0, "run nebmodule_init() -> %d", result);

    /* deinit neb module */
    dlerror();
    deinit_func = (int(*)(int,int))dlsym(neb_handle,"nebmodule_deinit");
    ok(deinit_func != NULL, "located nebmodule_deinit()");
    err = dlerror(); if(err != NULL) { BAIL_OUT("cannot load module: %s\n", err ); }

    result=deinit_func(NEBMODULE_FORCE_UNLOAD,NEBMODULE_NEB_SHUTDOWN);
    ok(result == 0, "run nebmodule_deinit() -> %d", result);
    err = dlerror(); if(err != NULL) { BAIL_OUT("cannot load module: %s\n", err ); }


    result=dlclose(neb_handle);
    ok(result == 0, "dlclose() -> %d", result);

    return;
}

#pragma GCC diagnostic pop    //Restore diagnostics state

/* core log wrapper */
void write_core_log(char *data);
void write_core_log(char *data) {
    printf("logger: %s", data);
    return;
}

/* fake some core functions */
int neb_set_module_info(__attribute__((unused)) void *handle, __attribute__((unused)) int type, __attribute__((unused)) char *data) { return 0; }
int neb_register_callback(__attribute__((__unused__)) enum NEBCallbackType callback_type, __attribute__((__unused__)) void *mod_handle, __attribute__((__unused__)) int priority, __attribute__((__unused__)) int (*callback_func)(int, void *)) { return 0; }
int neb_deregister_callback(__attribute__((__unused__)) enum NEBCallbackType callback_type, __attribute__((__unused__)) void *callback_func) { return 0; }

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
