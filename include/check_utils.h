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
 *  @brief utility components for all parts of mod_gearman
 *
 *  @{
 */

#include "popenRWE.h"
#include "common.h"

/**
 * nr2signal
 *
 * get name for a signal number
 *
 * @param[in] sig - signal number
 *
 * @return name for this signal
 */
char * nr2signal(int sig);

/**
 * extract_check_result
 *
 * get result from a file pointer
 *
 * @param[in] fp      - file pointer to executed command
 * @param[in] trimmed - trim result
 *
 * @return check result
 */
char *extract_check_result(FILE *fp, int trimmed);

/**
 * parse_command_line
 *
 * parse command line into argv array
 *
 * @param[in] cmd - command line
 * @param[out] argv - argv array
 *
 * @return true on success
 */
int parse_command_line(char *cmd, char *argv[GM_LISTSIZE]);

/**
 * run_check
 *
 * run a command
 *
 * @param[in] processed_command - command line
 * @param[out] plugin_output - pointer to plugin output
 * @param[out] plugin_error - pointer to plugin error output
 *
 * @return true on success
 */
int run_check(char *processed_command, char **plugin_output, char **plugin_error);

/**
 *
 * execute_safe_command
 *
 * execute command and fill the exec job structure
 *
 * @param[in] exec_job - job structure
 * @param[in] fork_exec - fork or not before exec
 * @param[in] identifier - current worker identifier
 *
 * @return true on success
 */
int execute_safe_command(gm_job_t * exec_job, int fork_exec, char * identifier);

/**
 *
 * check_alarm_handler
 *
 * called when a check runs into the timeout
 *
 * @param[in] sig - signal number
 *
 * @return nothing
 */
void check_alarm_handler(int sig);

/**
 * send_timeout_result
 *
 * send back a timeout result
 *
 * @param[in] exec_job - the exec job with all results
 *
 * @return nothing
 */
void send_timeout_result(gm_job_t * exec_job);

/**
 * @}
 */
