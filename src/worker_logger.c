/*****************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include "common.h"
#include "worker.h"
#include "worker_logger.h"

void logger( int lvl, const char *text, ... ) {

    // check log level
    if ( lvl != GM_LOG_ERROR && lvl > gearman_opt_debug_level ) {
        return;
    }

    char buffer[GM_BUFFERSIZE];
    time_t t = time(NULL);

    char * level;
    if ( lvl == GM_LOG_ERROR )
        level = "ERROR";
    else if ( lvl == GM_LOG_INFO )
        level = "INFO ";
    else if ( lvl == GM_LOG_DEBUG )
        level = "DEBUG";
    else if ( lvl == GM_LOG_TRACE )
        level = "TRACE";
    else
        level = "UNKNO";

    strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S]", localtime(&t) );

    char buffer2[GM_BUFFERSIZE];
    snprintf(buffer2, sizeof(buffer2), "[%i][%s] ", getpid(), level );
    strncat(buffer, buffer2, (sizeof(buffer)-1));

    va_list ap;
    va_start( ap, text );
    vsnprintf( buffer + strlen( buffer ), sizeof( buffer ) - strlen( buffer ), text, ap );
    va_end( ap );

    printf( "%s", buffer );
    return;
}
