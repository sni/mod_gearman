/******************************************************************************
 *
 * mod_gearman.c - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include <stdio.h>

#ifndef MOD_GM_COMMON_H
#define MOD_GM_COMMON_H

/* constants */
#define GM_VERSION                  "0.2"
#define GM_ENABLED                      1
#define GM_DISABLED                     0
#define GM_BUFFERSIZE                8192
#define GM_LISTSIZE                   512

#define GM_MIN_LIB_GEARMAN_VERSION   0.14
#define GM_SERVER_DEFAULT_PORT       4730

#define GM_OK                           0
#define GM_ERROR                        1

/* log modes */
#define GM_LOG_ERROR                   -1
#define GM_LOG_INFO                     0
#define GM_LOG_DEBUG                    1
#define GM_LOG_TRACE                    2
#define GM_LOG_STDOUT                   3

/* job priorities */
#define GM_JOB_PRIO_LOW                 1
#define GM_JOB_PRIO_NORMAL              2
#define GM_JOB_PRIO_HIGH                3

#define GM_DEFAULT_JOB_TIMEOUT         60
#define GM_DEFAULT_JOB_RETRIES          1
#define GM_CHILD_SHUTDOWN_TIMEOUT       5

/* worker */
#define GM_DEFAULT_MIN_WORKER           1      // minumum number of worker
#define GM_DEFAULT_MAX_WORKER          20      // maximum number of concurrent worker
#define GM_DEFAULT_JOB_MAX_AGE        600      // discard jobs older than that
#define GM_MAX_JOBS_PER_CLIENT         20

/* transport modes */
#define GM_ENCODE_AND_ENCRYPT           1
#define GM_ENCODE_ONLY                  2

/* modes */
#define GM_WORKER_MODE                  1
#define GM_NEB_MODE                     2


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

/* size of the shared memory segment */
#define GM_SHM_SIZE                   300

/* options structure */
typedef struct mod_gm_opt_struct {
    int            set_queues_by_hand;

    char         * crypt_key;
    char         * keyfile;
    char         * server_list[GM_LISTSIZE];
    int            server_num;
    char         * hostgroups_list[GM_LISTSIZE];
    int            hostgroups_num;
    char         * servicegroups_list[GM_LISTSIZE];
    int            servicegroups_num;
    int            debug_level;
    int            hosts;
    int            services;
    int            events;
    int            job_timeout;
    int            encryption;
    int            transportmode;
/* neb module */
    char         * result_queue;
    int            result_workers;
    int            perfdata;
    char         * local_hostgroups_list[GM_LISTSIZE];
    int            local_hostgroups_num;
    char         * local_servicegroups_list[GM_LISTSIZE];
    int            local_servicegroups_num;
/* worker */
    char         * pidfile;
    char         * logfile;
    FILE         * logfile_fp;
    int            daemon_mode;
    int            debug_result;
    int            max_age;
    int            min_worker;
    int            max_worker;
} mod_gm_opt_t;


/*
 * logger is then defined in worker_logger.c
 * and the neb logger in logger.c
 */
void logger( int lvl, const char *text, ... );


#endif
