/******************************************************************************
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

#define GM_JOB_PRIO_LOW                 1
#define GM_JOB_PRIO_NORMAL              2
#define GM_JOB_PRIO_HIGH                3

#define GM_DEFAULT_JOB_RETRIES          1

#ifndef TRUE
#define TRUE                            1
#elif (TRUE!=1)
#define TRUE                            1
#endif
#ifndef FALSE
#define FALSE                           0
#elif (FALSE!=0)
#define FALSE                           0
#endif

#define STATE_OK                        0
#define STATE_WARNING                   1
#define STATE_CRITICAL                  2
#define STATE_UNKNOWN                   3


#define GM_SHM_SIZE                   300
#define GM_SHM_KEY                   1234       // TODO: random key, verify if this makes problem with several independant worker on same host

void logger( int lvl, const char *text, ... );
