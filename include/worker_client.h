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
#define GM_WORKER_STATUS        2

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
    struct timeval core_start_time;
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
void *return_status( gearman_job_st *, void *, size_t *, gearman_return_t *);