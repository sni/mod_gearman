/*****************************************************************************
 *
 * mod_gearman.c - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include <sys/time.h>
#include <signal.h>
#include <libgearman/gearman.h>

#define GM_JOB_START            0
#define GM_JOB_END              1

#define GM_WORKER_MULTI         0
#define GM_WORKER_STANDALONE    1

typedef enum {
    GM_WORKER_OPTIONS_NONE=   0,
    GM_WORKER_OPTIONS_DATA=   ( 1 << 0 ),
    GM_WORKER_OPTIONS_STATUS= ( 1 << 1 ),
    GM_WORKER_OPTIONS_UNIQUE= ( 1 << 2 )
} gm_worker_options_t;

typedef struct gm_job_struct {
    char *         host_name;
    char *         service_description;
    char *         command_line;
    char *         type;
    char *         result_queue;
    char *         output;
    int            return_code;
    int            early_timeout;
    int            check_options;
    int            scheduled_check;
    int            reschedule_check;
    int            exited_ok;
    int            timeout;
    float          latency;
    struct timeval start_time;
    struct timeval finish_time;
} gm_job_t;

void *client_worker(int worker_mode);
void *worker_loop();
void *get_job( gearman_job_st *, void *, size_t *, gearman_return_t * );
int create_gearman_worker( gearman_worker_st *);
int create_gearman_client( gearman_client_st *client );
void *do_exec_job( gm_job_t * job );
void *send_result_back( gm_job_t * job );
void alarm_sighandler();
void send_state_to_parent(int status);
void *execute_safe_command(char**output, int *return_code, char *command_line, int timeout );
