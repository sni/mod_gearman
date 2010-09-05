/******************************************************************************
 *
 * mod_gearman.c - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <libgearman/gearman.h>

#define MOD_GM_WORKER
#include "common.h"

#define GM_JOB_START            0
#define GM_JOB_END              1

#define GM_WORKER_MULTI         0
#define GM_WORKER_STANDALONE    1

typedef struct gm_job_struct {
    char         * host_name;
    char         * service_description;
    char         * command_line;
    char         * type;
    char         * result_queue;
    char         * output;
    int            return_code;
    int            early_timeout;
    int            check_options;
    int            scheduled_check;
    int            reschedule_check;
    int            exited_ok;
    int            timeout;
    double         latency;
    struct timeval start_time;
    struct timeval finish_time;
} gm_job_t;

void worker_client(int worker_mode);
void worker_loop(void);
void *get_job( gearman_job_st *, void *, size_t *, gearman_return_t * );
void do_exec_job(void);
int set_worker( gearman_worker_st *worker );
void send_result_back(void);
void alarm_sighandler(int sig);
void send_state_to_parent(int status);
void execute_safe_command(void);
void clean_worker_exit(int signal);
