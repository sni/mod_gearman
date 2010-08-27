/*****************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include "worker.h"
#include "worker_logger.h"

void logger( int lvl, const char *text, ... ) {

    // check log level
    if ( lvl != GM_ERROR && lvl > gearman_opt_debug_level ) {
        return;
    }

    char buffer[8192];
    time_t t = time(NULL);
    strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S] ", localtime(&t) );

    if ( lvl == GM_ERROR ) {
        strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S] ERROR: ", localtime(&t) );
    } else {
        strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S] ", localtime(&t) );
    }

    va_list ap;
    va_start( ap, text );
    vsnprintf( buffer + strlen( buffer ), sizeof( buffer ) - strlen( buffer ), text, ap );
    va_end( ap );

    printf( "%s", buffer );
    return;
}
