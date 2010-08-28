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

/* escapes newlines in a string */
char *escape_newlines(char *rawbuf) {
    char *newbuf=NULL;
    register int x,y;

    if(rawbuf==NULL)
        return NULL;

    /* allocate enough memory to escape all chars if necessary */
    if((newbuf=malloc((strlen(rawbuf)*2)+1))==NULL)
        return NULL;

    for(x=0,y=0;rawbuf[x]!=(char)'\x0';x++){

        /* escape backslashes */
        if(rawbuf[x]=='\\'){
            newbuf[y++]='\\';
            newbuf[y++]='\\';
        }

        /* escape newlines */
        else if(rawbuf[x]=='\n'){
            newbuf[y++]='\\';
            newbuf[y++]='n';
        }

        else
            newbuf[y++]=rawbuf[x];
    }
    newbuf[y]='\x0';

    return newbuf;
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

/* convert exit code to int */
int real_exit_code(int code) {
    int return_code;
    if( code == -1 ){
        return_code   = 3;
    } else {
        if( WEXITSTATUS( code )== 0 && WIFSIGNALED( code) )
            return_code = 128 + WTERMSIG( code );
        else
            return_code = WEXITSTATUS( code );
    }
    return(return_code);
}
