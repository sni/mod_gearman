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
#include "crypt.h"
#include "base64.h"

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


/* convert exit code to int */
int real_exit_code(int code) {
    int return_code;
    if( code == -1 ){
        return_code   = 3;
    } else {
        if(WIFSIGNALED( code)) {
            return(128 + WTERMSIG( code ));
        }
        if(WIFEXITED(code)) {
            return(WEXITSTATUS(code));
        }
    }
    return(return_code);
}


/* initialize encryption */
void mod_gm_crypt_init(char * key) {
    if(strlen(key) < 8) {
        logger( GM_LOG_INFO, "encryption key should be at least 8 bytes!\n" );
    }
    return mod_gm_blowfish_init(key);
}


/* encrypt text with given key */
int mod_gm_encrypt(char ** encrypted, char * text, int mode) {
    int size;
    unsigned char * crypted;

    if(mode == GM_ENCODE_AND_ENCRYPT) {
        size = mod_gm_blowfish_encrypt(&crypted, text);
    }
    else {
        crypted = (unsigned char*)strdup(text);
        size    = strlen(text);
    }

    /* now encode in base64 */
    char * base64 = malloc(GM_BUFFERSIZE);
    base64[0] = 0;
    base64_encode(crypted, size, base64, GM_BUFFERSIZE);
    free(*encrypted);
    free(crypted);
    *encrypted = base64;
    return strlen(base64);
}


/* decrypt text with given key */
void mod_gm_decrypt(char ** decrypted, char * text, int mode) {
    unsigned char * buffer = malloc(GM_BUFFERSIZE);

    /* now decode from base64 */
    size_t bsize = base64_decode(text, buffer, GM_BUFFERSIZE);
    if(mode == GM_ENCODE_AND_ENCRYPT) {
        mod_gm_blowfish_decrypt(decrypted, buffer, bsize);
    }
    else  {
        decrypted = strdup((char*)buffer);
    }
    free(buffer);
    return;
}
