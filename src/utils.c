/*****************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include "common.h"
#include "utils.h"
#include "logger.h"

/* return string until token */
char *str_token( char **c, char delim ) {
    char *begin = *c;
    if ( !*begin ) {
        *c = begin;
        return 0;
    }

    char *end = begin;
    while ( *end && *end != delim ) end++;
    if ( *end ) {
        *end = 0;
        *c = end + 1;
    } else
        *c = end;
    return begin;
}


/* array push */
void push(gm_array_t *ps, void * data) {
    if (ps->size == GM_STACKSIZE) {
        logger(GM_LOG_ERROR, "Error: stack overflow\n");
        exit(1);
    } else
        ps->items[ps->size++] = data;
}


/* array pop */
void * pop(gm_array_t *ps) {
    if (ps->size == 0) {
        return NULL;
    } else
        return ps->items[--ps->size];
}
