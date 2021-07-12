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
 * @brief check_gearman naemon plugin
 * @addtogroup mod_gearman_check_gearman check_gearman
 *
 * check_gearman can be used as a naemon plugin to verify gearman server and worker.
 * It is part of the Mod-Gearman package but not limited to Mod-Gearman.
 *
 * @{
 */

#define MOD_GM_CHECK_GEARMAN             /**< set check_gearman mode */

#define PLUGIN_NAME    "check_gearman"   /**< set the name of the plugin */

#include <stdlib.h>
#include <signal.h>
#include "common.h"

/** check_gearman
 *
 * main function of check_gearman
 *
 * @param[in] argc - number of arguments
 * @param[in] argv - list of arguments
 *
 * @return exits with a naemon compatible exit code
 */
int main (int argc, char **argv);

/**
 *
 * print the usage and exit
 *
 * @return exits with a naemon compatible exit code
 */
void print_usage(void);

/**
 *
 * print the version and exit
 *
 * @return exits with a naemon compatible exit code
 */
void print_version(void);

/**
 *
 * signal handler for sig alarm
 *
 * @param[in] sig - signal number
 *
 * @return exits with a naemon compatible exit code
 */
void alarm_sighandler(int sig);

/**
 *
 * check a gearmand server
 *
 * @param[in] server - server to check
 *
 * @return returns a naemon compatible exit code
 */
int check_server(char * server, in_port_t port);

/**
 *
 * check a gearman worker
 *
 * @param[in] queue - queue name (function)
 * @param[in] send - put this text as job into the queue
 * @param[in] expect - returning text to expect
 *
 * @return returns a naemon compatible exit code
 */
int check_worker(char * queue, char * send, char * expect);

/**
 * @}
 */
