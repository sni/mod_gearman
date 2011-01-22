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

#define MOD_GM_WORKER   /**< set mod_gearman worker features */

#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <libgearman/gearman.h>
#include "common.h"

/** @file
 *  @brief Mod-Gearman Worker Client
 *  @addtogroup mod_gearman_worker Worker
 *
 *  Worker command line utility which executes the checks or eventhandler.
 *
 * @{
 */

int mod_gm_shm_key;        /**< key for the shared memory segment */
mod_gm_opt_t *mod_gm_opt;  /**< global options structure          */

/** Mod-Gearman Worker
 *
 * main function of the worker
 *
 * @param[in] argc - number of arguments
 * @param[in] argv - list of arguments
 *
 * @return when worker exits, returns exit code of worker
 */
int main (int argc, char **argv);

/**
 * store the commandline to parse it again on reloading the worker
 *
 * @param[in] argc - number of arguments
 * @param[in] argv - list of arguments
 *
 * @return TRUE on success or FALSE if not
 */
int store_original_comandline(int argc, char **argv);

/**
 * parse the arguments into the global options structure
 *
 * @param[in] argc - number of arguments
 * @param[in] argv - list of arguments
 *
 * @return TRUE on success or FALSE if not
 */
int parse_arguments(int argc, char **argv);

/**
 * create a new child process
 *
 * @param[in] mode - mode for the new child
 *
 * @return TRUE on success or FALSE if not
 */
int make_new_child(int mode);

/**
 * print the usage and exit
 *
 * @return nothing and exits
 */
void print_usage(void);

/**
 * print the version and exit
 *
 * @return nothing and exits
 */
void print_version(void);

/**
 * calculate the new number of child worker
 *
 * @param[in] min         - minimum number of worker
 * @param[in] max         - maximum number of worker
 * @param[in] cur_workers - current number of worker
 * @param[in] cur_jobs    - current number of running jobs
 *
 * @return new target number of workers
 */
int  adjust_number_of_worker(int min, int max, int cur_workers, int cur_jobs);

/**
 * creates the shared memory segments for the child communication
 *
 * @return nothing
 */
void setup_child_communicator(void);

/**
 * finish and clean all childs and shared memory segments, then exit.
 *
 * @param[in] sig - signal which caused the exit
 *
 * @return nothing
 */
void clean_exit(int sig);

/**
 * write the current pid into the pidfile
 *
 * @return TRUE on success or FALSE if something went wrong
 */
int write_pid_file(void);

/**
 * verify options structure and check for missing options
 *
 * @param[in] opt - options structure to verify
 *
 * @return TRUE on success or FALSE if something went wrong
 */
int verify_options(mod_gm_opt_t *opt);

/**
 * signal handler for the HUP signal
 *
 * @param[in] sig - signal
 *
 * @return nothing
 */
void reload_config(int sig);

/**
 * stop all child
 *
 * @param[in] mode - could be stop or restart
 *
 * @return nothing
 */
void stop_childs(int mode);

/**
 * main loop to maintain the child population
 *
 * @return nothing
 */
void monitor_loop(void);

/**
 * check and start new worker childs if level is too low
 *
 * @return nothing
 */
void check_worker_population(void);

/**
 * returns next number of the shared memory segment for a new child
 *
 * @return nothing
 */
int get_next_shm_index(void);

/**
 * count and set the current number of worker
 *
 * @return nothing
 */
void count_current_worker(void);

/**
 * @}
 */