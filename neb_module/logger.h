/*****************************************************************************
 *
 * mod_gearman.c - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

// defaults to the NSLOG_INFO_MESSAGE level
#define GM_ERROR     0
#define GM_INFO      0
#define GM_DEBUG     1
#define GM_TRACE     2

void logger(int lvl, const char *text, ...);
