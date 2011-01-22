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

/**
 * @file
 * @brief send_gearman command line utility
 * @addtogroup mod_gearman_send_gearman send_gearman
 *
 * command line utility to send results back to core. Replacement for send_nsca.
 *
 * @{
 */

#define MOD_GM_SEND_GEARMAN

#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <libgearman/gearman.h>
#include "common.h"

mod_gm_opt_t *mod_gm_opt;            /**< global options structure */


/** send_gearman
 *
 * main function of gearman_top
 *
 * @param[in] argc - number of arguments
 * @param[in] argv - list of arguments
 *
 * @return just exits
 */
int main (int argc, char **argv);

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
 *
 * print the usage and exit
 *
 * @return exits with a nagios compatible exit code
 */
void print_usage(void);

/**
 *
 * print the version and exit
 *
 * @return exits with a nagios compatible exit code
 */
void print_version(void);

/**
 * verify options structure and check for missing options
 *
 * @param[in] opt - options structure to verify
 *
 * @return TRUE on success or FALSE if something went wrong
 */
int verify_options(mod_gm_opt_t *opt);

/**
 * send_result
 *
 * create and send back the gearman jobs
 *
 * @return TRUE on success or FALSE if something went wrong
 */
int send_result(void);

/**
 * alarm_sighandler
 *
 * handles sig alarms
 *
 * @param[in] sig - signal number
 *
 * @return nothing
 */
void alarm_sighandler(int sig);


/**
 * @}
 */