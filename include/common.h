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

/** \mainpage Mod-Gearman
 *
 * \section intro_sec Introduction
 *
 * Mod-Gearman (http://labs.consol.de/nagios/mod-gearman) is a new
 * way of distributing active Naemon checks (http://www.naemon.org) across your network. It
 * consists of two parts: There is a NEB module which resides in the
 * Naemon core and adds servicechecks, hostchecks and eventhandler to a
 * Gearman (http://gearman.org) queue. There can be multiple equal
 * gearman servers.  The counterpart is one or more worker clients for
 * the checks itself. They can be bound to host and servicegroups.
 *
 * @attention Please submit bugreports or patches on https://github.com/sni/mod_gearman
 * @author Sven Nierlein <sven.nierlein@consol.de>
 *
 */


/** @file
 *  @brief common header for all components
 *
 *  @{
 */

#include <config.h>
#include <stdio.h>
#include <sys/time.h>
#include <arpa/inet.h>

#ifndef MOD_GM_COMMON_H
#define MOD_GM_COMMON_H

/* constants */
#define GM_VERSION                 "2.0.0"
#define GM_ENABLED                      1
#define GM_DISABLED                     0
#define GM_BUFFERSIZE               65536
#define GM_LISTSIZE                   512
#define GM_NEBTYPESSIZE                33   /* maximum number of neb types */

#define GM_MIN_LIB_GEARMAN_VERSION   0.14
#define GM_SERVER_DEFAULT_PORT       4730

#define GM_OK                           0
#define GM_ERROR                        1
#define GM_NO_EPN                      -1

#define GM_EXIT_UNKNOWN               768   /* results in exit code 3 after processed by WEXITSTATUS() */

/* log modes */
#define GM_LOG_ERROR                   -1
#define GM_LOG_INFO                     0
#define GM_LOG_DEBUG                    1
#define GM_LOG_TRACE                    2
#define GM_LOG_TRACE2                   3
#define GM_LOG_TRACE3                   4
#define GM_LOG_STDOUT                   5

/* log modes */
#define GM_LOG_MODE_AUTO                0
#define GM_LOG_MODE_FILE                1
#define GM_LOG_MODE_STDOUT              2
#define GM_LOG_MODE_CORE                3
#define GM_LOG_MODE_SYSLOG              4
#define GM_LOG_MODE_TOOLS               5

/* job priorities */
#define GM_JOB_PRIO_LOW                 1
#define GM_JOB_PRIO_NORMAL              2
#define GM_JOB_PRIO_HIGH                3

#define GM_DEFAULT_JOB_TIMEOUT         60
#define GM_DEFAULT_JOB_RETRIES          1
#define GM_CHILD_SHUTDOWN_TIMEOUT      10
#define GM_DEFAULT_RESULT_QUEUE  "check_results"
#define GM_DEFAULT_IDLE_TIMEOUT        10
#define GM_DEFAULT_MAX_JOBS          1000
#define MAX_CMD_ARGS                 4096

/* worker */
#define GM_DEFAULT_MIN_WORKER           1      /**< minumum number of worker             */
#define GM_DEFAULT_MAX_WORKER          20      /**< maximum number of concurrent worker  */
#define GM_DEFAULT_JOB_MAX_AGE          0      /**< discard jobs older than that         */
#define GM_DEFAULT_SPAWN_RATE           1      /**< number of spawned worker per seconds */
#define GM_DEFAULT_WORKER_LOOP_SLEEP    1      /**< sleep in worker main loop */

/* transport modes */
#define GM_ENCODE_AND_ENCRYPT           1
#define GM_ENCODE_ONLY                  2
#define GM_ENCODE_ACCEPT_ALL            3

/* dump config modes */
#define GM_WORKER_MODE                  1
#define GM_NEB_MODE                     2
#define GM_SEND_GEARMAN_MODE            3

/* worker stop modes */
#define GM_WORKER_STOP                  1
#define GM_WORKER_RESTART               2

/* perfdata modes */
#define GM_PERFDATA_OVERWRITE           1
#define GM_PERFDATA_APPEND              2


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

#define STATE_OK                        0    /**< core exit code for ok       */
#define STATE_WARNING                   1    /**< core exit code for warning  */
#define STATE_CRITICAL                  2    /**< core exit code for critical */
#define STATE_UNKNOWN                   3    /**< core exit code for unknown  */

#define GM_SHM_SIZE                  4096    /**< size of the shared memory segment */

/** options exports structure
 *
 * structure for export definition
 *
 */
typedef struct mod_gm_export {
    char   * name[GM_LISTSIZE];             /**< list of queue names to export into */
    int      return_code[GM_LISTSIZE];      /**< list of return codes which should be returned to naemon */
    int      elem_number;                   /**< number of elements */
} mod_gm_exp_t;

/** server structure
 *
 * structure for server definition
 *
 */
typedef struct gm_server {
    char            * host;                 /**< hostname of server */
    in_port_t       port;                   /**< port number */
} gm_server_t;

/** options structure
 *
 * structure union for all components
 * all config files and commandline arguments are parsed
 * into this structure
 */
typedef struct mod_gm_opt_struct {
    int            set_queues_by_hand;                      /**< flag whether there has been queues configured by hand */

    char         * crypt_key;                               /**< encryption key used for securing the messages sent over gearman */
    char         * keyfile;                                 /**< path to a file where the crypt_key is read from */
    gm_server_t  * server_list[GM_LISTSIZE];                /**< list of gearmand servers */
    int            server_num;                              /**< number of gearmand servers */
    gm_server_t  * dupserver_list[GM_LISTSIZE];             /**< list of gearmand servers to duplicate results */
    int            dupserver_num;                           /**< number of duplicate gearmand servers */
    char         * hostgroups_list[GM_LISTSIZE];            /**< list of hostgroups which get own queues */
    int            hostgroups_num;                          /**< number of elements in hostgroups_list */
    char         * servicegroups_list[GM_LISTSIZE];         /**< list of servicegroups which get own queues */
    int            servicegroups_num;                       /**< number of elements in servicegroups_list */
    int            debug_level;                             /**< level of debug output */
    int            hosts;                                   /**< flag wheter host checks are distributed or not */
    int            services;                                /**< flag wheter service checks are distributed or not */
    int            events;                                  /**< flag wheter eventhandlers are distributed or not */
    int            job_timeout;                             /**< override job timeout */
    int            encryption;                              /**< flag wheter messages are encrypted */
    int            transportmode;                           /**< flag for the transportmode, base64 only or base64 and encrypted  */
    int            logmode;                                 /**< logmode: auto, syslog, file or core */
    char         * logfile;                                 /**< path for the logfile */
    FILE         * logfile_fp;                              /**< filedescriptor for the logfile */
    int            use_uniq_jobs;                           /**< flag whether normal jobs will be sent with/without uniq set */
/* neb module */
    char         * result_queue;                            /**< name of the result queue used by the neb module */
    int            result_workers;                          /**< number of result worker threads started */
    int            perfdata;                                /**< flag whether perfdata will be distributed or not */
    int            perfdata_mode;                           /**< flag whether perfdata will be sent with/without uniq set */
    char         * local_hostgroups_list[GM_LISTSIZE];      /**< list of hostgroups which will not be distributed */
    int            local_hostgroups_num;                    /**< number of elements in local_hostgroups_list */
    char         * local_servicegroups_list[GM_LISTSIZE];   /**< list of group  which will not be distributed */
    int            local_servicegroups_num;                 /**< number of elements in local_servicegroups_list */
    int            do_hostchecks;                           /**< flag whether mod-gearman will process hostchecks at all */
    int            route_eventhandler_like_checks;          /**< flag whether mod-gearman will route like normal checks */
    char         * queue_cust_var;                          /**< custom variable name which contains the target queue */
    mod_gm_exp_t * exports[GM_NEBTYPESSIZE];                /**< list of exporter queues */
    int            exports_count;                           /**< number of export queues */
    int            orphan_host_checks;                      /**< generate fake result for orphaned host checks */
    int            orphan_service_checks;                   /**< generate fake result for orphaned service checks */
    int            accept_clear_results;                    /**< accept unencrypted results */
/* worker */
    char         * identifier;                              /**< identifier for this worker */
    char         * pidfile;                                 /**< path to a pidfile */
    int            daemon_mode;                             /**< running as daemon ot not? */
    int            debug_result;                            /**< flag to write a debug file for each result */
    int            max_age;                                 /**< max age in seconds for new jobs */
    int            min_worker;                              /**< minimum number of workers */
    int            max_worker;                              /**< maximum number of workers */
    int            fork_on_exec;                            /**< flag to disable additional forks for each job */
    int            idle_timeout;                            /**< number of seconds till a idle worker exits */
    int            max_jobs;                                /**< maximum number of jobs done after a worker exits */
    int            spawn_rate;                              /**< number of spawned new worker */
    int            show_error_output;                       /**< optional display the stderr output of plugins */
    int            timeout_return;                          /**< timeout return code */
    int            orphan_return;                           /**< orphan return code */
    int            dup_results_are_passive;                 /**< send duplicate results as passive checks */
    double         load_limit1;                             /**< load limit 1min for new worker */
    double         load_limit5;                             /**< load limit 5min for new worker */
    double         load_limit15;                            /**< load limit 15min for new worker */
#ifdef EMBEDDEDPERL
    int            enable_embedded_perl;                    /**< enabled embedded perl */
    int            use_embedded_perl_implicitly;            /**< use embedded perl implicitly */
    int            use_perl_cache;                          /**< cache embedded perl scripts */
    char         * p1_file;                                 /**< path to p1 file, needed for embedded perl */
#endif
    char         * restrict_path[GM_LISTSIZE];              /**< list of path restrictions */
    int            restrict_path_num;                       /**< number of path restrictions */
    int            workaround_rc_25;                        /**< optional workaround for plugins returning exit code 25 */
/* send_gearman */
    int            timeout;                                 /**< timeout for waiting reading on stdin */
    int            return_code;                             /**< return code */
    char         * message;                                 /**< message output */
    char         * host;                                    /**< hostname for this check */
    char         * service;                                 /**< service description for this check */
    char         * delimiter;                               /**< delimiter to cut check results */
    int            active;                                  /**< flag wheter the result is a active or a passive check */
    int            has_starttime;                           /**< flag when starttime is set */
    struct timeval starttime;                               /**< time when the check started */
    int            has_finishtime;                          /**< flag when finishtime is set */
    struct timeval finishtime;                              /**< time when the check finished */
    int            has_latency;                             /**< flag when latency is set */
    struct timeval latency;                                 /**< latency for this result */
} mod_gm_opt_t;


/** structure for jobs to execute */
typedef struct gm_job_struct {
    char         * host_name;           /**< hostname for this job */
    char         * service_description; /**< service description for this job or NULL */
    char         * command_line;        /**< command line to execute */
    char         * type;                /**< type of this job */
    char         * result_queue;        /**< name of the result queue */
    char         * output;              /**< output from the executed command line (stdout) */
    char         * error;               /**< errors from the executed command line (stderr) */
    char         * source;              /**< source of this check */
    int            return_code;         /**< return code for this job */
    int            early_timeout;       /**< did the check run into a timeout */
    int            check_options;       /**< check_options given from the core */
    int            scheduled_check;     /**< normal scheduled check? */
    int            reschedule_check;    /**< rescheduled check? */
    int            exited_ok;           /**< did the plugin exit normally? */
    int            timeout;             /**< timeout for this job */
    double         latency;             /**< latency for from this job */
    struct timeval next_check;          /**< next_check value of host / service */
    struct timeval core_time;           /**< time when the core started the job */
    struct timeval start_time;          /**< time when the job really started */
    struct timeval finish_time;         /**< time when the job was finished */
    int            has_been_sent;       /**< flag if job has been sent back */
} gm_job_t;


/** options structure */
mod_gm_opt_t *mod_gm_opt;
gm_job_t * current_job;
char hostname[GM_BUFFERSIZE];

/*
 * @}
 */

#endif
