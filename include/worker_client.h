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

/** @file
 *  @brief header for mod-gearman worker client component
 *
 *  @{
 */

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

/** structure for jobs to execute */
typedef struct gm_job_struct {
    char         * host_name;           /**< hostname for this job */
    char         * service_description; /**< service description for this job or NULL */
    char         * command_line;        /**< command line to execute */
    char         * type;                /**< type of this job */
    char         * result_queue;        /**< name of the result queue */
    char         * output;              /**< output from the executed command line */
    int            return_code;         /**< return code for this job */
    int            early_timeout;       /**< did the check run into a timeout */
    int            check_options;       /**< check_options given from the core */
    int            scheduled_check;     /**< normal scheduled check? */
    int            reschedule_check;    /**< rescheduled check? */
    int            exited_ok;           /**< did the plugin exit normally? */
    int            timeout;             /**< timeout for this job */
    double         latency;             /**< latency for from this job */
    struct timeval core_start_time;     /**< time when the core started the job */
    struct timeval start_time;          /**< time when the job really started */
    struct timeval finish_time;         /**< time when the job was finished */
} gm_job_t;

void worker_client(int worker_mode, int index, int shid);
void worker_loop(void);
void *get_job( gearman_job_st *, void *, size_t *, gearman_return_t * );
void do_exec_job(void);
int set_worker( gearman_worker_st *worker );
void send_result_back(void);
void alarm_sighandler(int sig);
void idle_sighandler(int sig);
void set_state(int status);
void execute_safe_command(void);
void clean_worker_exit(int sig);
void *return_status( gearman_job_st *, void *, size_t *, gearman_return_t *);
int set_default_job(gm_job_t *job);
int free_job(gm_job_t *job);
#ifdef GM_DEBUG
void write_debug_file(char ** text);
#endif


/**
 * @}
 */