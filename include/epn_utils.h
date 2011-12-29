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

#include "common.h"

/** @file
 *  @brief embedded perl utility components for all parts of mod_gearman
 *
 *  @{
 */

/**
 * run_epn_check
 *
 * run a check with epn when available
 *
 * @param[in] processed_command - command line
 * @param[out] plugin_output - pointer to plugin output
 * @param[out] plugin_error - pointer to plugin error output
 *
 * @return true/false
 */
int run_epn_check(char *processed_command, char **ret, char **err);

/**
 * file_uses_embedded_perl
 *
 * tests whether or not the embedded perl interpreter should be used on a file
 *
 * @param[in] file - path to file
 *
 * @return true/false
 */
int file_uses_embedded_perl(char *);

/**
 * init_embedded_perl
 *
 * initialize embedded perl interpreter
 *
 * @param[in] env - environment
 *
 * @return true
 */

int init_embedded_perl(char **);

/**
 * deinit_embedded_perl
 *
 * deinitialize embedded perl interpreter
 *
 * @return true
 */

int deinit_embedded_perl(void);

/**
 * @}
 */
