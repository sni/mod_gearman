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

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "polarssl/md5.h"
#include "common.h"

/**
 * escpae newlines
 *
 * @param[in] rawbuf  - text to escape
 * @param[in] trimmed - trim string before escaping
 *
 * @return a text with all newlines escaped
 */
char *gm_escape_newlines(char *rawbuf, int trimmed);

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
 * double2timeval
 *
 * parse double into timeval
 *
 * @param[in] value - double value
 * @param[out] t - pointer to timeval structure
 *
 * @return nothing
 */
void double2timeval(double value, struct timeval * t);

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
 *
 * set_default_job
 *
 * fill defaults into job structure
 *
 * @param[in] job - job structure to be filled
 * @param[in] mod_gm_opt - options structure
 *
 * @return true on success
 */
int set_default_job(gm_job_t *job, mod_gm_opt_t *mod_gm_opt);

/**
 *
 * free_job
 *
 * free job structure
 *
 * @param[in] job - job structure to be freed
 *
 * @return true on success
 */
int free_job(gm_job_t *job);

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
 * escapestring
 *
 * escape quotes and newlines
 *
 * @param[in] rawbuf - text to escape
 *
 * @return the escaped string
 */
char *escapestring(char *rawbuf);

/**
 * escaped
 *
 * checks wheter a char has to be escaped or not
 *
 * @param[in] ch - char to check
 *
 * @return true if char has to be escaped
 */
int escaped(int ch);

/**
 * escape
 *
 * return escaped variant of char
 *
 * @param[out] out - escaped char
 * @param[in] ch - char to escape
 *
 * @return the escaped string
 */
void escape(char *out, int ch);

/**
 * nebtype2str
 *
 * get human readable name for neb type
 *
 * @param[in] i - integer to translate
 *
 * @return the human readable string
 */
char * nebtype2str(int i);


/**
 * nebcallback2str
 *
 * get human readable name for nebcallback type int
 *
 * @param[in] i - integer to translate
 *
 * @return the human readable string
 */
char * nebcallback2str(int i);

/**
 * eventtype2str
 *
 * get human readable name for eventtype type int
 *
 * @param[in] i - integer to translate
 *
 * @return the human readable string
 */
char * eventtype2str(int i);

/**
 * gm_log
 *
 * general logger
 *
 * @param[in] lvl  - debug level for this message
 * @param[in] text - text to log
 *
 * @return nothing
 */
void gm_log( int lvl, const char *text, ... );

/**
 * write_core_log
 *
 * write log line with core logger
 *
 * @param[in] data - log message
 *
 * @return nothing
 */
void write_core_log(char *data);

/**
 * get_param_server
 *
 * return string of new server or NULL on duplicate
 *
 * @param[in] servername - server name to parse
 * @param[in] server_list - list of servers to check for duplicates
 * @param[in] server_num - number of server in this list
 *
 * @returns the new server name or NULL
 */
char * get_param_server(char * servername, char * server_list[GM_LISTSIZE], int server_num);

/**
 * send_result_back
 *
 * send back result
 *
 * @param[in] exec_job - the exec job with all results
 *
 * @return nothing
 */
void send_result_back(gm_job_t * exec_job);

/**
 * md5sum
 *
 * create md5 sum
 *
 * @param[in] text - char array to get md5 from
 *
 * @return md5sum (hex)
 */
char *md5sum(char *text);

/**
 * @}
 */
