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

#define MOD_GM_WORKER

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

int mod_gm_shm_key;
mod_gm_opt_t *mod_gm_opt;

int main (int argc, char **argv);
int store_original_comandline(int argc, char **argv);
int parse_arguments(int argc, char **argv);
int make_new_child(int mode);
void print_usage(void);
void print_version(void);
int  adjust_number_of_worker(int min, int max, int cur_workers, int cur_jobs);
void check_signal(int sig);
void setup_child_communicator(void);
void clean_exit(int sig);
int write_pid_file(void);
int verify_options(mod_gm_opt_t *opt);
void reload_config(int sig);
void stop_childs(int mode);
void update_runtime_data(void);
void monitor_loop(void);
void wait_sighandler(int sig);
void check_worker_population(void);
