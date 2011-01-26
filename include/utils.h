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

char *escape_newlines(char *rawbuf);
int real_exit_code(int code);
void mod_gm_crypt_init(char * key);
int mod_gm_encrypt(char ** encrypted, char * text, int mode);
void mod_gm_decrypt(char ** decrypted, char * text, int mode);
int file_exists (char * fileName);
char *ltrim(char *s);
char *rtrim(char *s);
char *trim(char *s);
int set_default_options(mod_gm_opt_t *opt);
char *lc(char * str);
int parse_args_line(mod_gm_opt_t *opt, char * arg, int recursion_level);
int parse_yes_or_no(char*value, int dfl);
int read_config_file(mod_gm_opt_t *opt, char*filename, int recursion_level);
void dumpconfig(mod_gm_opt_t *opt, int mode);
void mod_gm_free_opt(mod_gm_opt_t *opt);
int read_keyfile(mod_gm_opt_t *opt);
char * nr2signal(int sig);
void string2timeval(char * value, struct timeval * t);
double timeval2double(struct timeval * t);
long mod_gm_time_compare(struct timeval * tv1, struct timeval * tv2);
char *extract_check_result(FILE *fp);
int parse_command_line(char *cmd, char *argv[GM_LISTSIZE]);
int run_check(char *processed_command, char **plugin_output);
int pid_alive(int pid);
