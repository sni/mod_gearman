/******************************************************************************
 *
 * mod_gearman.c - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include "common.h"

char *str_token( char **c, char delim );
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
void parse_args_line(mod_gm_opt_t *opt, char * arg);
int parse_yes_or_no(char*value, int dfl);
void read_config_file(mod_gm_opt_t *opt, char*filename);
void dumpconfig(mod_gm_opt_t *opt);
