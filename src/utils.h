/*****************************************************************************
 *
 * mod_gearman.c - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#define GM_STACKSIZE 65536

typedef struct {
    int size;
    void * items[GM_STACKSIZE];
} gm_array_t;

char *str_token( char **c, char delim );
void push(gm_array_t *ps, void * data);
void * pop(gm_array_t *ps);
char *escape_newlines(char *rawbuf);
