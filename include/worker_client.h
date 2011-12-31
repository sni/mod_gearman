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
#include "config.h"
#include "common.h"

#define GM_JOB_START            0
#define GM_JOB_END              1

#define GM_WORKER_MULTI         0
#define GM_WORKER_STANDALONE    1
#define GM_WORKER_STATUS        2

#ifdef EMBEDDEDPERL
void worker_client(int worker_mode, int indx, int shid, char**env);
#else
void worker_client(int worker_mode, int indx, int shid);
#endif
void worker_loop(void);
void *get_job( gearman_job_st *, void *, size_t *, gearman_return_t * );
void do_exec_job(void);
int set_worker( gearman_worker_st *worker );
void idle_sighandler(int sig);
void set_state(int status);
void clean_worker_exit(int sig);
void *return_status( gearman_job_st *, void *, size_t *, gearman_return_t *);
#ifdef GM_DEBUG
void write_debug_file(char ** text);
#endif


/**
 * @}
 */
