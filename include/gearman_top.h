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
 * @brief gearman_top command line utility
 * @addtogroup mod_gearman_gearman_top gearman_top
 *
 * Command line utility which connects to the admin interface of a gearman daemon.
 * displays current worker and queue utilization.
 *
 * @{
 */

#include <stdlib.h>
#include <signal.h>
#include <curses.h>
#include <time.h>
#include "common.h"

/** gearman_top
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
 *
 * close all connections and exit
 *
 * @param[in] sig - signal number received
 *
 * @return just exits
 */
void clean_exit(int sig);

/**
 *
 * print the usage and exit
 *
 * @return just exits
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
 *
 * print the statistics for a given hostname
 *
 * @param[in] hostname - hostname to connect to
 *
 * @return nothing
 */
void print_stats(char * hostname);

/**
 * @}
 */
