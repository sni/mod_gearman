/*****************************************************************************
 *
 * mod_gearman.c - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include "nagios/nagios.h"

// defaults to the NSLOG_INFO_MESSAGE level
#define GM_ERROR    -1
#define GM_INFO      0
#define GM_DEBUG     1
#define GM_TRACE     2
#define GM_STDOUT    3

void logger( int lvl, const char *text, ... );
