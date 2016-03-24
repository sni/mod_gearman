#include <ltdl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

#ifdef USENAEMON
#include "naemon/naemon.h"
#include "naemon/nebmodules.h"
#include "naemon/nebstructs.h"
#include "naemon/nebcallbacks.h"
#include "naemon/broker.h"
#endif
#ifdef USENAEMON
#include "naemon/naemon.h"
#include "naemon/nebmodules.h"
#include "naemon/nebstructs.h"
#include "naemon/nebcallbacks.h"
#include "naemon/broker.h"
#endif

static void init_externals(void);
static void* load_neb(char* objfile, char * nebargs);
static void unload_neb(void* neb_handle);
static host* setup_host(char* hst_name);
static host* init_host(void);
static service* init_svc(void);
static service* setup_svc(host* hst, char* svc_name);
static nebstruct_service_check_data* init_svc_check(void);
static nebstruct_service_check_data* setup_service(char* host_name,
        char* service_name);
static int reap_cr_from_neb(int num_of_pending_results);
static int verify_check_results(int num_of_pending_results);

#define MAX_NEB_CB 3
#define MAX_CONCURRENT_CHECKS 500

/* core log wrapper */
void write_core_log(char *data);

/* some externals required by the neb module, originally located in naemon core */
int service_check_timeout;
int host_check_timeout;
int currently_running_service_checks;
int currently_running_host_checks;
#ifdef USENAEMON
int event_broker_options;
#endif
#ifdef USENAEMON
unsigned long event_broker_options;
#endif
check_result *check_result_list;
check_result check_result_info;
int process_performance_data;

nebcallback nebcb_process_data[MAX_NEB_CB];
nebcallback nebcb_timed_event_data[MAX_NEB_CB];
nebcallback nebcb_service_check_data[MAX_NEB_CB];

typedef struct check_result_queue {
    char host_name[64];
    char service_description[64];
    char output[128];
    struct timeval start_time;
    struct timeval finish_time;
} check_result_data;

host this_hst;
service this_svc;
nebstruct_service_check_data this_data_service_check;
objectlist *cr_container[MAX_CONCURRENT_CHECKS];
int nr_of_rcv_results = 0;

static void init_externals(void) {
    /* set some external variables */
    service_check_timeout = 30;
    host_check_timeout = 30;
    currently_running_service_checks = 0;
    currently_running_host_checks = 0;
    event_broker_options = 1048575; /* BROKER_EVERYTHING */
    check_result_list = NULL;
    check_result_info.check_options = 1; /* CHECK_OPTION_FORCE_EXECUTION */
    process_performance_data = 1;
}

/* Note: neb module will register NEBCALLBACK_PROCESS_DATA and NEBCALLBACK_TIMED_EVENT_DATA during this call. */
static void* load_neb(char* objfile, char * nebargs) {
    int (*initfunc)(int, char *, void *);
    int *module_version_ptr = NULL;
    void* neb_handle;
    int result;
    char *err;

    /* load neb module */
    neb_handle = (void *) dlopen(objfile, RTLD_LAZY | RTLD_GLOBAL);
    err = dlerror();
    if (err != NULL) {
        printf("cannot load module: %s\n", err);
        exit(-1);
    }
    module_version_ptr = (int *) dlsym(neb_handle, "__neb_api_version");
    if ((*module_version_ptr) == CURRENT_NEB_API_VERSION)
        printf("got module api version %i", CURRENT_NEB_API_VERSION);

    /* init neb module */
    initfunc = dlsym(neb_handle, "nebmodule_init");
    err = dlerror();
    if (err != NULL) {
        printf("cannot load module: %s\n", err);
        exit(-1);
    }

    result = (*initfunc)(NEBMODULE_NORMAL_LOAD, nebargs, neb_handle);
    if (result == 0)
        printf("run nebmodule_init() -> %d\n", result);
    err = dlerror();
    if (err != NULL) {
        printf("cannot load module: %s\n", err);
        exit(-1);
    }

    return neb_handle;
}

static void unload_neb(void* neb_handle) {
    int (*deinitfunc)(int, int) = NULL;
    int result;
    char *err;

    /* deinit neb module */
    deinitfunc = (void *) dlsym(neb_handle, "nebmodule_deinit");
    err = dlerror();
    if (err != NULL) {
        printf("cannot load module: %s\n", err );
        exit(-1);
    }

    result = (*deinitfunc)(NEBMODULE_FORCE_UNLOAD, NEBMODULE_NEB_SHUTDOWN);
    err = dlerror();
    if (result || err != NULL) {
        printf("cannot load module: %s\n", err );
        exit(-1);
    }

    result = dlclose(neb_handle);

    return;
}

/* declared in naemon/nebmodules.h */
int neb_set_module_info(void *handle, int type, char *data) {
    (void) handle;
    (void) type;
    (void) data;
    return 0;
}

/* declared in naemon/nebcallbacks.h */
int neb_deregister_callback(int callback_type,
        int (*callback_func)(int, void *)) {
    (void) callback_type;
    (void) callback_func;
    return 0;
}

/* declared in naemon/nebmods.h */
int neb_make_callbacks(int callback_type, void *data) {
    int (*callbackfunc)(int, void *);
    register int cbresult = 0;
    int i;

    /* make sure the callback type is within bounds */
    if (callback_type < 0 || callback_type >= NEBCALLBACK_NUMITEMS)
        return ERROR;

    switch (callback_type) {
    case NEBCALLBACK_PROCESS_DATA:
        for (i = 0; i < MAX_NEB_CB; i++)
            if (nebcb_process_data[i].callback_func != NULL) {
                callbackfunc = nebcb_process_data[i].callback_func;
                cbresult = callbackfunc(NEBCALLBACK_PROCESS_DATA, data);
            }
        break;
    case NEBCALLBACK_TIMED_EVENT_DATA:
        for (i = 0; i < MAX_NEB_CB; i++)
            if (nebcb_timed_event_data[i].callback_func != NULL) {
                callbackfunc = nebcb_timed_event_data[i].callback_func;
                cbresult = callbackfunc(NEBCALLBACK_TIMED_EVENT_DATA, data);
            }
        break;
    case NEBCALLBACK_SERVICE_CHECK_DATA:
        for (i = 0; i < MAX_NEB_CB; i++)
            if (nebcb_service_check_data[i].callback_func != NULL) {
                callbackfunc = nebcb_service_check_data[i].callback_func;
                cbresult = callbackfunc(NEBCALLBACK_SERVICE_CHECK_DATA, data);
            }
        break;
    }

    return cbresult;
}

/* allows a module to register a callback function */
int neb_register_callback(int callback_type, void *mod_handle, int priority,
        int (*callback_func)(int, void *)) {
    nebcallback *new_callback = NULL;
    int i;

    if (callback_func == NULL)
        return NEBERROR_NOCALLBACKFUNC;

    if (mod_handle == NULL)
        return NEBERROR_NOMODULEHANDLE;

    /* make sure the callback type is within bounds */
    if (callback_type < 0 || callback_type >= NEBCALLBACK_NUMITEMS)
        return NEBERROR_CALLBACKBOUNDS;

    switch (callback_type) {
    case NEBCALLBACK_PROCESS_DATA:
        for (i = 0; i < MAX_NEB_CB; i++)
            if (nebcb_process_data[i].callback_func == NULL) {
                printf(
                        "neb module registered a function for NEBCALLBACK_PROCESS_DATA.\n");
                new_callback = &nebcb_process_data[i];
                break;
            }
        break;
    case NEBCALLBACK_TIMED_EVENT_DATA:
        for (i = 0; i < MAX_NEB_CB; i++)
            if (nebcb_timed_event_data[i].callback_func == NULL) {
                printf(
                        "neb module registered a function for NEBCALLBACK_TIMED_EVENT_DATA.\n");
                new_callback = &nebcb_timed_event_data[i];
                break;
            }
        break;
    case NEBCALLBACK_SERVICE_CHECK_DATA:
        for (i = 0; i < MAX_NEB_CB; i++)
            if (nebcb_service_check_data[i].callback_func == NULL) {
                printf(
                        "neb module registered function for NEBCALLBACK_SERVICE_CHECK_DATA.\n");
                new_callback = &nebcb_service_check_data[i];
                break;
            }
        break;
    default:
        printf("unhandled registration for type %i\n", callback_type);
        return 0;
    }

    new_callback->priority = priority;
    new_callback->module_handle = mod_handle;
    new_callback->callback_func = callback_func;
    return 0;
}

/* include/utils.h */
void write_core_log(char *data) {
    printf("logger: %s", data);
    return;
}

/* mod_gearman/include/naemon/macros.h */
int clear_volatile_macros(void) {
    return 0;
}

/* declared in include/naemon/macros.h */
int grab_host_macros(host *hst) {
    (void) hst;
    return 0;
}

/* declared in include/naemon/macros.h */
int grab_service_macros(service *svc) {
    (void) svc;
    return 0;
}

/* declared in include/naemon/utils.h */
int get_raw_command_line(command *cmd_ptr, char *cmd, char **full_command,
        int macro_options) {
    (void) macro_options;
    (void) cmd;
    *full_command = malloc(strlen(cmd_ptr->command_line) + 1);
    strncpy(*full_command, cmd_ptr->command_line,
            strlen(cmd_ptr->command_line));

    return 0;
}

/* declared in include/naemon/macros.h */
int process_macros(char *input_buffer, char **output_buffer, int options) {
    (void) options;
    *output_buffer = malloc(strlen(input_buffer) + 1);
    strncpy(*output_buffer, input_buffer, strlen(input_buffer));

    return 0;
}

/* declared in include/naemon/objects.h */
struct service *find_service(const char * hst, const char * svc) {
    (void) hst;
    (void) svc;
    return &this_svc;
}

/* declared in include/naemon/utils.h */
int init_check_result(check_result *info) {

    if (info == NULL)
        return ERROR;

    /* reset vars */
    info->object_check_type = HOST_CHECK;
    info->host_name = NULL;
    info->service_description = NULL;
    info->check_type = HOST_CHECK_ACTIVE;
    info->check_options = CHECK_OPTION_NONE;
    info->scheduled_check = FALSE;
    info->reschedule_check = FALSE;
    info->output_file_fp = NULL;
    info->latency = 0.0;
    info->start_time.tv_sec = 0;
    info->start_time.tv_usec = 0;
    info->finish_time.tv_sec = 0;
    info->finish_time.tv_usec = 0;
    info->early_timeout = FALSE;
    info->exited_ok = TRUE;
    info->return_code = 0;
    info->output = NULL;

    return OK;
}

static host* setup_host(char* hst_name) {
    host* hst = &this_hst;
    static char hst_name_buf[256];

    hst->name = hst_name_buf;
    strncpy(hst->name, hst_name, sizeof(hst_name_buf));

    return hst;
}

/* Init some default values for test, inspired by a gdb data dump. Most of this data is never touched during this test. */
static host* init_host(void) {
    static servicesmember services;
    static command check_command_ptr;
    host* hst;

    services.host_name = 0x0;
    services.service_description = 0x0;
    services.service_ptr = 0x0;
    services.next = 0x0;
    check_command_ptr.name = "";
    check_command_ptr.command_line = "printf my_host_check";
    check_command_ptr.next = 0x0;

    hst = &this_hst;
    hst->name = NULL;
    hst->display_name = NULL;
    hst->alias = NULL;
    hst->address = NULL;
    hst->parent_hosts = 0x0;
    hst->child_hosts = 0x0;
    hst->services = &services;
    hst->initial_state = 0;
    hst->check_interval = 1;
    hst->retry_interval = 1;
    hst->max_attempts = 10;
    hst->event_handler = 0x0;
    hst->contact_groups = 0x0;
    hst->contacts = 0x0;
    hst->notification_interval = 30;
    hst->first_notification_delay = 0;
    hst->notification_period = "24x7";
    hst->check_period = "24x7";
    hst->flap_detection_enabled = 1;
    hst->low_flap_threshold = 0;
    hst->high_flap_threshold = 0;
    hst->check_freshness = 0;
    hst->freshness_threshold = 0;
    hst->process_performance_data = 1;
    hst->checks_enabled = 1;
    hst->event_handler_enabled = 1;
    hst->retain_status_information = 1;
    hst->retain_nonstatus_information = 1;
    hst->notes = 0x0;
    hst->notes_url = 0x0;
    hst->action_url = 0x0;
    hst->icon_image = "";
    hst->icon_image_alt = 0x0;
    hst->vrml_image = 0x0;
    hst->statusmap_image = 0x0;
    hst->have_2d_coords = 0;
    hst->x_2d = -1;
    hst->y_2d = -1;
    hst->have_3d_coords = 0;
    hst->x_3d = 0;
    hst->y_3d = 0;
    hst->z_3d = 0;
    hst->should_be_drawn = 1;
    hst->custom_variables = 0x0;
    hst->problem_has_been_acknowledged = 0;
    hst->acknowledgement_type = 0;
    hst->check_type = 0;
    hst->current_state = 0;
    hst->last_state = 0;
    hst->last_hard_state = 0;
    hst->plugin_output = NULL;
    hst->long_plugin_output = 0x0;
    hst->perf_data = NULL;
    hst->state_type = 1;
    hst->current_attempt = 1;
    hst->current_event_id = 0;
    hst->last_event_id = 0;
    hst->current_problem_id = 0;
    hst->last_problem_id = 0;
    hst->latency = 0;
    hst->execution_time = 0;
    hst->is_executing = 0;
    hst->check_options = 0;
    hst->notifications_enabled = 1;
    hst->next_check = 0;
    hst->should_be_scheduled = 1;
    hst->last_check = 0;
    hst->last_state_change = 0;
    hst->last_hard_state_change = 0;
    hst->last_time_up = 0;
    hst->last_time_down = 0;
    hst->last_time_unreachable = 0;
    hst->has_been_checked = 1;
    hst->is_being_freshened = 0;
    hst->current_notification_number = 0;
    hst->no_more_notifications = 0;
    hst->current_notification_id = 0;
    hst->check_flapping_recovery_notification = 0;
    hst->scheduled_downtime_depth = 0;
    hst->pending_flex_downtime = 0;
    memset(hst->state_history, 0, sizeof(hst->state_history));
    hst->state_history_index = 1;
    hst->last_state_history_update = 1416578907;
    hst->is_flapping = 0;
    hst->flapping_comment_id = 0;
    hst->percent_state_change = 0;
    hst->total_services = 1;
    hst->total_service_check_interval = 1;
    hst->modified_attributes = 0;
    hst->event_handler_ptr = 0x0;
    hst->check_command_ptr = &check_command_ptr;
    hst->check_period_ptr = NULL;
    hst->notification_period_ptr = NULL;
    hst->hostgroups_ptr = NULL;
    hst->next = 0x0;

    return hst;
}

static service* setup_svc(host* hst, char* svc_name) {
    static char svc_name_buf[256];
    service* svc;

    svc = &this_svc;
    svc->description = svc_name_buf;
    strncpy(svc->description, svc_name, sizeof(svc_name_buf));
    svc->host_ptr = hst;
    svc->host_name = hst->name;

    return svc;
}

/* Init some default values for test, inspired by a gdb data dump. Most of this data is never touched during this test. */
static service* init_svc()
{
    service* svc;

    static int state_history[MAX_STATE_HISTORY_ENTRIES];
    static command check_command;

    static char output[128] = "";
    static char long_output[256] = "";

    check_command.name = "";
    check_command.command_line = NULL;
    check_command.next = NULL;

    svc = &this_svc;
    svc->host_name = "TestHost";
    svc->description = "";
    svc->display_name = "";
    svc->event_handler = 0x0;
    svc->initial_state = 0;
    svc->check_interval = 1;
    svc->retry_interval = 1;
    svc->max_attempts = 3;
    svc->contact_groups = NULL;
    svc->contacts = 0x0;
    svc->notification_interval = 60;
    svc->first_notification_delay = 0;
    svc->is_volatile = 0;
    svc->notification_period = "24x7";
    svc->check_period = "24x7";
    svc->flap_detection_enabled = 1;
    svc->low_flap_threshold = 0;
    svc->high_flap_threshold = 0;
    svc->process_performance_data = 1;
    svc->check_freshness = 0;
    svc->freshness_threshold = 0;
    svc->event_handler_enabled = 1;
    svc->checks_enabled = 1;
    svc->retain_status_information = 1;
    svc->retain_nonstatus_information = 1;
    svc->notifications_enabled = 0;
    svc->notes = 0x0;
    svc->notes_url = 0x0;
    svc->action_url = 0x0;
    svc->icon_image = "";
    svc->icon_image_alt = 0x0;
    svc->custom_variables = 0x0;
    svc->problem_has_been_acknowledged = 0;
    svc->acknowledgement_type = 0;
    svc->host_problem_at_last_check = 0;
    svc->check_type = 0;
    svc->current_state = 2;
    svc->last_state = 2;
    svc->last_hard_state = 2;
    svc->plugin_output = output;
    svc->long_plugin_output = long_output;
    svc->perf_data = "";
    svc->state_type = 1;
    svc->next_check = 0;;
    svc->should_be_scheduled = 1;
    svc->last_check = 0;;
    svc->current_attempt = 3;
    svc->current_event_id = 1;
    svc->last_event_id = 0;
    svc->current_problem_id = 0;
    svc->last_problem_id = 0;
    svc->last_notification = 0;
    svc->next_notification = 0;
    svc->no_more_notifications = 0;
    svc->check_flapping_recovery_notification = 0;
    svc->last_state_change = 0;
    svc->last_hard_state_change = 0;
    svc->last_time_ok = 0;
    svc->last_time_warning = 0;
    svc->last_time_unknown = 0;
    svc->last_time_critical = 0;
    svc->has_been_checked = 1;
    svc->is_being_freshened = 0;
    svc->current_notification_number = 0;
    svc->current_notification_id = 0;
    svc->latency = 0;
    svc->execution_time = 0;
    svc->is_executing = 0;
    svc->check_options = 0;
    svc->scheduled_downtime_depth = 0;
    svc->pending_flex_downtime = 0;
    memcpy(svc->state_history, state_history, sizeof(state_history));
    svc->state_history_index = 0;
    svc->is_flapping = 0;
    svc->flapping_comment_id = 0;
    svc->percent_state_change = 5.46;
    svc->modified_attributes = 0;
    svc->host_ptr = NULL;
    svc->event_handler_ptr = 0x0;
    svc->event_handler_args = 0x0;
    check_command.command_line = (char*) malloc(128);
    svc->check_command_ptr = &check_command;
    svc->check_command_args = 0x0;
    svc->check_period_ptr = NULL;
    svc->notification_period_ptr = NULL;
    svc->servicegroups_ptr = 0x0;
    svc->next = 0x0;

    return svc;
}

/* Init some default values for test, inspired by a gdb data dump. Most of this data is never touched during this test. */
static nebstruct_service_check_data* init_svc_check(void) {
    nebstruct_service_check_data* data_service_check = &this_data_service_check;

    data_service_check->type = 704;
    data_service_check->flags = 0;
    data_service_check->attr = 0;
    data_service_check->timestamp.tv_sec = 0;
    data_service_check->timestamp.tv_usec = 0;
    data_service_check->host_name = NULL;
    data_service_check->service_description = NULL;
    data_service_check->check_type = 0;
    data_service_check->current_attempt = 0;
    data_service_check->max_attempts = 0;
    data_service_check->state_type = 0;
    data_service_check->state = 0;
    data_service_check->timeout = 0;
    data_service_check->command_name = "check_testcheck";
    data_service_check->command_args = 0x0;
    data_service_check->command_line = 0x0;
    data_service_check->start_time.tv_sec = 0;
    data_service_check->start_time.tv_usec = 0;
    data_service_check->end_time.tv_sec = 0;
    data_service_check->end_time.tv_usec = 0;
    data_service_check->early_timeout = 0;
    data_service_check->execution_time = 0;
    data_service_check->latency = 0;
    data_service_check->return_code = 0;
    data_service_check->output = NULL;
    data_service_check->long_output = NULL;
    data_service_check->perf_data = NULL;
    data_service_check->object_ptr = NULL;

    return data_service_check;
}

static nebstruct_service_check_data* setup_service(char* host_name,
        char* service_name) {
    nebstruct_service_check_data* data_service_check;
    service* svc;

    data_service_check = &this_data_service_check;
    svc = setup_svc(setup_host(host_name), service_name);

    strcpy(svc->check_command_ptr->command_line, "printf \"$(date +%s%N)\\n\"");

    data_service_check->object_ptr = svc;
    gettimeofday(&data_service_check->timestamp, NULL);
    data_service_check->host_name = svc->host_name;
    data_service_check->service_description = svc->description;
    data_service_check->current_attempt = svc->current_attempt;
    data_service_check->max_attempts = svc->max_attempts;
    data_service_check->state = svc->current_state;
    data_service_check->state_type = svc->state_type;
    data_service_check->output = svc->plugin_output;
    data_service_check->long_output = svc->long_plugin_output;
    data_service_check->perf_data = svc->perf_data;

    return data_service_check;
}

/* declared in naemon/utils.h */
int process_check_result(check_result * cr) {
    long check_idx;
    char* endptr;

    if (cr->service_description && *cr->service_description != '\0') {
        check_result_data* returned_result;

        check_idx = strtol(cr->service_description, &endptr, 10);
        returned_result = gm_malloc(sizeof(check_result_data));
        if (cr->host_name)
        {
            strncpy(returned_result->host_name, cr->host_name,
                    sizeof(returned_result->host_name) - 1);
        }
        if (cr->service_description)
        {
            strncpy(returned_result->service_description,
                    cr->service_description,
                    sizeof(returned_result->service_description) - 1);
        }
        if (cr->output) {
            char* p_term;
            strncpy(returned_result->output, cr->output,
                    sizeof(returned_result->output) - 1);
            returned_result->output[127] = '\0';
            p_term = strchr(returned_result->output, '\n');
            if (p_term)
                *p_term = '\0';
        }
        returned_result->start_time = cr->start_time;
        returned_result->finish_time = cr->finish_time;

        add_object_to_objectlist(&cr_container[check_idx], returned_result);
        nr_of_rcv_results++;
    } else {
        printf("Spurious check result detected\n");
    }

    return 0;
}

/* declared in naemon/utils.h */
int free_check_result(check_result * cr) {
    if (cr) {
        free(cr->host_name);
        cr->host_name = NULL;
        free(cr->service_description);
        cr->service_description = NULL;
        free(cr->source);
        cr->source = NULL;
        free(cr->output);
        cr->output = NULL;
    }

    return 0;
}

/* declared in naemon/objects.h */
int add_object_to_objectlist(objectlist **list, void *object_ptr) {
    objectlist *temp_item = NULL;
    objectlist *new_item = NULL;

    if (list == NULL || object_ptr == NULL)
        return ERROR;

    /* skip this object if its already in the list */
    for (temp_item = *list; temp_item; temp_item = temp_item->next) {
        if (temp_item->object_ptr == object_ptr)
            break;
    }
    if (temp_item)
        return OK;

    /* allocate memory for a new list item */
    if ((new_item = (objectlist *) malloc(sizeof(objectlist))) == NULL)
        return ERROR;

    /* initialize vars */
    new_item->object_ptr = object_ptr;

    /* add new item to head of list */
    new_item->next = *list;
    *list = new_item;

    return OK;
}

static int reap_cr_from_neb(int num_of_pending_results) {
    nebstruct_timed_event_data data_timed_event;
    int tmp_nr_of_results = -1;

    printf("Waiting for all results to arrive...\n");
    while (tmp_nr_of_results != nr_of_rcv_results
            || nr_of_rcv_results < num_of_pending_results) {
        if (tmp_nr_of_results >= 0)
            printf(" received %i from %i\n", nr_of_rcv_results,
                    num_of_pending_results);

        tmp_nr_of_results = nr_of_rcv_results;

        /* reap results, neb module will call process_check_result therefore */
        data_timed_event.event_type = EVENT_CHECK_REAPER;
        neb_make_callbacks(NEBCALLBACK_TIMED_EVENT_DATA, &data_timed_event);
        sleep(1);
    }
    nr_of_rcv_results = 0;

    return verify_check_results(num_of_pending_results);
}

static int verify_check_results(int num_of_pending_results) {
    int check_idx;
    int num_results = 0;

    for (check_idx = 0; check_idx < num_of_pending_results; check_idx++) {
        objectlist *returned_result_list = cr_container[check_idx];
        objectlist *tmp_list = NULL;

        if (returned_result_list) {
            char output[2][64] = { "", "" };
            int duplicates = 0;
            for ((void) returned_result_list; returned_result_list;
                    returned_result_list = returned_result_list->next) {
                check_result_data* tmp_returned_result =
                        (check_result_data*) returned_result_list->object_ptr;
                free(tmp_list);
                strncpy(output[duplicates], tmp_returned_result->output, 63);
                free(returned_result_list->object_ptr);
                tmp_list = returned_result_list;
                duplicates++;
            }
            if (duplicates > 1) {
                printf(
                        "duplicated results (%i) for check %i detected. Timestamp 1 [%s], Timestamp 2 [%s]\n",
                        duplicates, check_idx, output[0], output[1]);
                /*printf("(first desc[%s]-stamp[%i]-out[%s], this desc[%s]-stamp[%i]-out[%s])\n",
                 service_list_ack[svcnum].service_description, service_list_ack[svcnum].start_time.tv_sec ,service_list_ack[svcnum].output,
                 iter->service_description, iter->start_time.tv_sec, iter->output);*/
            }
            cr_container[check_idx] = 0;
            num_results += duplicates;
        } else {
            printf("no result returned for check %i\n", check_idx);
        }
    }

    printf("verifying %i results done\n", num_results);

    return num_results;
}

int main(void) {
    /* typical in 10000 service scenario: 150..1000 concurrently scheduled async jobs. */
    const int num_checks_at_once = MAX_CONCURRENT_CHECKS;
    void* neb_handle;
    int i;
    char ok = 1;
    char * test_nebargs[] = { "config=extras/shared.conf", };

    struct nebstruct_process_struct data_process_events;
    data_process_events.type = NEBTYPE_PROCESS_EVENTLOOPSTART;

    init_externals();

    init_host();
    init_svc();
    init_svc_check();

    neb_handle = load_neb("./mod_gearman.o", test_nebargs[0]);
    neb_make_callbacks(NEBCALLBACK_PROCESS_DATA, &data_process_events);

    while (ok) {
        printf("Sending %i service checks.\n", num_checks_at_once);
        for (i = 0; i < num_checks_at_once; i++) {
            nebstruct_service_check_data* data_service_check;
            char host_name[16];
            char service_name[16];
            sprintf(host_name, "%i", i / 10);
            sprintf(service_name, "%i", i);
            data_service_check = setup_service(host_name, service_name);
            neb_make_callbacks(NEBCALLBACK_SERVICE_CHECK_DATA,
                    data_service_check);
        }

        ok = (reap_cr_from_neb(num_checks_at_once) >= num_checks_at_once);
    }

    unload_neb(neb_handle);

    return 0;
}
