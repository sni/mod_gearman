/*****************************************************************************
 *
 * mod_gearman.c - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

/* constants */
#define GM_VERSION                  "0.1"
#define GM_ENABLED                      1
#define GM_DISABLED                     0
#define GM_BUFFERSIZE                8192
#define GM_LISTSIZE                   512

#define GM_MIN_LIB_GEARMAN_VERSION   0.14
#define GM_SERVER_DEFAULT_PORT       4730

#define GM_OK                           0
#define GM_ERROR                        1

#define GM_LOG_ERROR                   -1
#define GM_LOG_INFO                     0
#define GM_LOG_DEBUG                    1
#define GM_LOG_TRACE                    2
#define GM_LOG_STDOUT                   3