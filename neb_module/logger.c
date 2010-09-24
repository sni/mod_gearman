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
#include "mod_gearman.h"
#include "logger.h"

void logger( int lvl, const char *text, ... ) {
    char buffer[GM_BUFFERSIZE];
    va_list ap;

    /* check log level */
    if ( lvl != GM_LOG_ERROR && lvl > mod_gm_opt->debug_level ) {
        return;
    }

    if ( lvl == GM_LOG_ERROR ) {
        snprintf( buffer, 22, "mod_gearman: ERROR - " );
    } else {
        snprintf( buffer, 14, "mod_gearman: " );
    }
    va_start( ap, text );
    vsnprintf( buffer + strlen( buffer ), sizeof( buffer ) - strlen( buffer ), text, ap );
    va_end( ap );

    /* in case of stdout logging just print and return */
    if ( mod_gm_opt->debug_level >= GM_LOG_STDOUT ) {
        printf( "%s", buffer );
        return;
    }

    /* send everything as info message to the core */
    write_to_all_logs( buffer, NSLOG_INFO_MESSAGE );
}
