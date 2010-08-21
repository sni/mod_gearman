/*****************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include "mod_gearman.h"
#include "logger.h"

void logger(int lvl, const char *text, ...) {
    char buffer[8192];
    snprintf(buffer, 14, "mod_gearman: ");

    // check log level
    if(lvl != GM_ERROR && lvl > gearman_opt_debug_level) {
        return;
    }

    va_list ap;
    va_start(ap, text);
    vsnprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), text, ap);
    va_end(ap);

    // in case of debug, log to stdout too
    if(lvl >= GM_DEBUG) {
        printf("%s", buffer);
    }

    // send everything as info message to the core
    write_to_all_logs(buffer, NSLOG_INFO_MESSAGE);
}
