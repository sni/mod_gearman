/*****************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include "common.h"
#include "mod_gearman.h"
#include "logger.h"

void logger( int lvl, const char *text, ... ) {

    // check log level
    if ( lvl != GM_LOG_ERROR && lvl > gearman_opt_debug_level ) {
        return;
    }

    char buffer[8192];
    if ( lvl == GM_LOG_ERROR ) {
        snprintf( buffer, 22, "mod_gearman: ERROR - " );
    } else {
        snprintf( buffer, 14, "mod_gearman: " );
    }
    va_list ap;
    va_start( ap, text );
    vsnprintf( buffer + strlen( buffer ), sizeof( buffer ) - strlen( buffer ), text, ap );
    va_end( ap );

    // in case of stdout logging just print and return
    if ( gearman_opt_debug_level >= GM_LOG_STDOUT ) {
        printf( "%s", buffer );
        return;
    }

    // send everything as info message to the core
    write_to_all_logs( buffer, NSLOG_INFO_MESSAGE );
}
