/******************************************************************************
 *
 * mod_gearman.c - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

char *str_token( char **c, char delim );
char *escape_newlines(char *rawbuf);
int real_exit_code(int code);
void mod_gm_crypt_init(char * key);
int mod_gm_encrypt(char ** encrypted, char * text);
void mod_gm_decrypt(char ** decrypted, char * text, int size);
