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

#include "common.h"

/**
 * escpae newlines
 *
 * @param[in] rawbuf - text to escape
 *
 * @return a text with all newlines escaped
 */
char *escape_newlines(char *rawbuf);

/**
 * real_exit_code
 *
 * converts a exit from wait() into real number
 *
 * @param[in] code - exit code
 *
 * @return real exit code
 */
int real_exit_code(int code);

/**
 * mod_gm_crypt_init
 *
 * wrapper to initialze mod_gearman crypt module
 *
 * @param[in] key - password for encryption
 *
 * @return nothing
 */
void mod_gm_crypt_init(char * key);

/**
 * mod_gm_encrypt
 *
 * wrapper to encrypt text
 *
 * @param[out] encrypted - pointer to encrypted text
 * @param[in] text - text to encrypt
 * @param[in] mode - encryption mode (base64 or aes64 with base64)
 *
 * @return base64 encoded text or aes encrypted text based on mode
 */
int mod_gm_encrypt(char ** encrypted, char * text, int mode);

/**
 * mod_gm_decrypt
 *
 * @param[out] decrypted - pointer to decrypted text
 * @param[in] text - text to decrypt
 * @param[in] mode - do only base64 decoding or decryption too
 *
 * @return decrypted text
 */
void mod_gm_decrypt(char ** decrypted, char * text, int mode);

/**
 * file_exists
 *
 * @param[in] fileName - path to file
 *
 * @return true if file exists
 */
int file_exists (char * fileName);

/**
 * ltrim
 *
 * trim whitespace to the left
 *
 * @param[in] s - text to trim
 *
 * @return trimmed text
 */
char *ltrim(char *s);

/**
 * rtrim
 *
 * trim whitespace to the right
 *
 * @param[in] s - text to trim
 *
 * @return trimmed text
 */
char *rtrim(char *s);

/**
 * trim
 *
 * trim whitespace from left and right
 *
 * @param[in] s - text to trim
 *
 * @return trimmed text
 */
char *trim(char *s);

/**
 * set_default_options
 *
 * fill in default options
 *
 * @param[in] opt - option structure
 *
 * @return true on success
 */
int set_default_options(mod_gm_opt_t *opt);

/**
 * lc
 *
 * lowercase given text
 *
 * @param[in] str - text to lowercase
 *
 * @return lowercased text
 */
char *lc(char * str);

/**
 * parse_args_line
 *
 * parse the command line arguments in our options structure
 *
 * @param[in] opt - options structure
 * @param[in] arg - arguments
 * @param[in] recursion_level - counter for the recursion level
 *
 * @return true on success
 */
int parse_args_line(mod_gm_opt_t *opt, char * arg, int recursion_level);

/**
 * parse_yes_or_no
 *
 * parse a string for yes/no, on/off
 *
 * @param[in] value - string to parse
 * @param[in] dfl - default value if none matches
 *
 * @return parsed value
 */
int parse_yes_or_no(char*value, int dfl);

/**
 * read_config_file
 *
 * read config options from a file
 *
 * @param[in] opt - options structure
 * @param[in] filename - file to parse
 * @param[in] recursion_level - counter for the recursion level
 *
 * @return true on success
 */
int read_config_file(mod_gm_opt_t *opt, char*filename, int recursion_level);

/**
 * dumpconfig
 *
 * dumps config with logger
 *
 * @param[in] opt - options structure
 * @param[in] mode - display mode
 *
 * @return nothing
 */
void dumpconfig(mod_gm_opt_t *opt, int mode);

/**
 * mod_gm_free_opt
 *
 * free options structure
 *
 * @param[in] opt - options structure
 *
 * @return nothing
 */
void mod_gm_free_opt(mod_gm_opt_t *opt);

/**
 * read_keyfile
 *
 * read keyfile into options structure
 *
 * @param[in] opt - options structure
 *
 * @return true on success
 */
int read_keyfile(mod_gm_opt_t *opt);

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
 * string2timeval
 *
 * parse string into timeval
 *
 * @param[in] value - string to parse
 * @param[out] t - pointer to timeval structure
 *
 * @return nothing
 */
void string2timeval(char * value, struct timeval * t);

/**
 * timeval2double
 *
 * convert timeval into double
 *
 * @param[in] t - timeval structure
 *
 * @return double value for this timeval structure
 */
double timeval2double(struct timeval * t);

/**
 * mod_gm_time_compare
 *
 * get difference between two timeval
 *
 * @param[in] tv1 - first timeval
 * @param[in] tv2 - second timeval
 *
 * @return difference in seconds
 */
long mod_gm_time_compare(struct timeval * tv1, struct timeval * tv2);

/**
 * extract_check_result
 *
 * get result from a file pointer
 *
 * @param[in] fp - file pointer to executed command
 *
 * @return check result
 */
char *extract_check_result(FILE *fp);

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
 *
 * @return true on success
 */
int run_check(char *processed_command, char **plugin_output);

/**
 * pid_alive
 *
 * check if a pid is alive
 *
 * @param[in] pid - pid to check
 *
 * @return true if pid is alive
 */
int pid_alive(int pid);

/**
 * @}
 */