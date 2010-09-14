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
#include "worker.h"
#include "worker_logger.h"

struct tm now;

void logger( int lvl, const char *text, ... ) {

    FILE * fp       = NULL;
    int debug_level = GM_LOG_ERROR;
    if(mod_gm_opt != NULL) {
        debug_level = mod_gm_opt->debug_level;
        fp          = mod_gm_opt->logfile_fp;
    }

    // check log level
    if ( lvl != GM_LOG_ERROR && lvl > debug_level ) {
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

    now = *(localtime(&t));

    strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S]", &now );

    char buffer2[GM_BUFFERSIZE];
    snprintf(buffer2, sizeof(buffer2), "[%i][%s] ", getpid(), level );
    strncat(buffer, buffer2, (sizeof(buffer)-1));

    va_list ap;
    va_start( ap, text );
    vsnprintf( buffer + strlen( buffer ), sizeof( buffer ) - strlen( buffer ), text, ap );
    va_end( ap );

    if(fp != NULL) {
        fprintf( fp, "%s", buffer );
        fflush( fp );
    } else {
        printf( "%s", buffer );
    }
    return;
}
