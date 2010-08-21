/* Gearman server and library
 * Copyright (C) 2008 Brian Aker, Eric Day
 * All rights reserved.
 *
 * Use and distribution licensed under the BSD license.  See
 * the COPYING file in the parent directory for full text.
 */

/**
 * @file
 * @brief Defines, typedefs, and enums
 */

#ifndef __GEARMAN_CONSTANTS_H__
#define __GEARMAN_CONSTANTS_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup gearman_constants Constants
 * @ingroup gearman_universal
 * @ingroup gearman_client
 * @ingroup gearman_worker
 * @{
 */

/* Defines. */
#define GEARMAN_DEFAULT_TCP_HOST "127.0.0.1"
#define GEARMAN_DEFAULT_TCP_PORT 4730
#define GEARMAN_DEFAULT_SOCKET_TIMEOUT 10
#define GEARMAN_DEFAULT_SOCKET_SEND_SIZE 32768
#define GEARMAN_DEFAULT_SOCKET_RECV_SIZE 32768
#define GEARMAN_MAX_ERROR_SIZE 1024
#define GEARMAN_PACKET_HEADER_SIZE 12
#define GEARMAN_JOB_HANDLE_SIZE 64
#define GEARMAN_OPTION_SIZE 64
#define GEARMAN_UNIQUE_SIZE 64
#define GEARMAN_MAX_COMMAND_ARGS 8
#define GEARMAN_ARGS_BUFFER_SIZE 128
#define GEARMAN_SEND_BUFFER_SIZE 8192
#define GEARMAN_RECV_BUFFER_SIZE 8192
#define GEARMAN_WORKER_WAIT_TIMEOUT (10 * 1000) /* Milliseconds */

/**
 * Return codes.
 */
typedef enum
{
  GEARMAN_SUCCESS,
  GEARMAN_IO_WAIT,
  GEARMAN_SHUTDOWN,
  GEARMAN_SHUTDOWN_GRACEFUL,
  GEARMAN_ERRNO,
  GEARMAN_EVENT,
  GEARMAN_TOO_MANY_ARGS,
  GEARMAN_NO_ACTIVE_FDS,
  GEARMAN_INVALID_MAGIC,
  GEARMAN_INVALID_COMMAND,
  GEARMAN_INVALID_PACKET,
  GEARMAN_UNEXPECTED_PACKET,
  GEARMAN_GETADDRINFO,
  GEARMAN_NO_SERVERS,
  GEARMAN_LOST_CONNECTION,
  GEARMAN_MEMORY_ALLOCATION_FAILURE,
  GEARMAN_JOB_EXISTS,
  GEARMAN_JOB_QUEUE_FULL,
  GEARMAN_SERVER_ERROR,
  GEARMAN_WORK_ERROR,
  GEARMAN_WORK_DATA,
  GEARMAN_WORK_WARNING,
  GEARMAN_WORK_STATUS,
  GEARMAN_WORK_EXCEPTION,
  GEARMAN_WORK_FAIL,
  GEARMAN_NOT_CONNECTED,
  GEARMAN_COULD_NOT_CONNECT,
  GEARMAN_SEND_IN_PROGRESS,
  GEARMAN_RECV_IN_PROGRESS,
  GEARMAN_NOT_FLUSHING,
  GEARMAN_DATA_TOO_LARGE,
  GEARMAN_INVALID_FUNCTION_NAME,
  GEARMAN_INVALID_WORKER_FUNCTION,
  GEARMAN_NO_REGISTERED_FUNCTION,
  GEARMAN_NO_REGISTERED_FUNCTIONS,
  GEARMAN_NO_JOBS,
  GEARMAN_ECHO_DATA_CORRUPTION,
  GEARMAN_NEED_WORKLOAD_FN,
  GEARMAN_PAUSE,
  GEARMAN_UNKNOWN_STATE,
  GEARMAN_PTHREAD,
  GEARMAN_PIPE_EOF,
  GEARMAN_QUEUE_ERROR,
  GEARMAN_FLUSH_DATA,
  GEARMAN_SEND_BUFFER_TOO_SMALL,
  GEARMAN_IGNORE_PACKET,
  GEARMAN_UNKNOWN_OPTION,
  GEARMAN_TIMEOUT,
  GEARMAN_ARGUMENT_TOO_LARGE,
  GEARMAN_MAX_RETURN /* Always add new error code before */
} gearman_return_t;

/**
 * Verbosity levels.
 */
typedef enum
{
  GEARMAN_VERBOSE_NEVER,
  GEARMAN_VERBOSE_FATAL,
  GEARMAN_VERBOSE_ERROR,
  GEARMAN_VERBOSE_INFO,
  GEARMAN_VERBOSE_DEBUG,
  GEARMAN_VERBOSE_CRAZY,
  GEARMAN_VERBOSE_MAX
} gearman_verbose_t;

/** @} */

/**
 * @ingroup gearman_universal
 * Options for gearman_universal_st.
 */
typedef enum
{
  GEARMAN_NON_BLOCKING,
  GEARMAN_DONT_TRACK_PACKETS,
  GEARMAN_MAX
} gearman_universal_options_t;

/**
 * @ingroup gearman_con
 * Options for gearman_connection_st.
 */
typedef enum
{
  GEARMAN_CON_READY,
  GEARMAN_CON_PACKET_IN_USE,
  GEARMAN_CON_EXTERNAL_FD,
  GEARMAN_CON_IGNORE_LOST_CONNECTION,
  GEARMAN_CON_CLOSE_AFTER_FLUSH,
  GEARMAN_CON_MAX
} gearman_connection_options_t;

/**
 * @ingroup gearman_packet
 * Command types. When you add a new entry, update gearman_command_info_list in
 * packet.c as well.
 */
typedef enum
{
  GEARMAN_COMMAND_TEXT,
  GEARMAN_COMMAND_CAN_DO,              /* W->J: FUNC */
  GEARMAN_COMMAND_CANT_DO,             /* W->J: FUNC */
  GEARMAN_COMMAND_RESET_ABILITIES,     /* W->J: -- */
  GEARMAN_COMMAND_PRE_SLEEP,           /* W->J: -- */
  GEARMAN_COMMAND_UNUSED,
  GEARMAN_COMMAND_NOOP,                /* J->W: -- */
  GEARMAN_COMMAND_SUBMIT_JOB,          /* C->J: FUNC[0]UNIQ[0]ARGS */
  GEARMAN_COMMAND_JOB_CREATED,         /* J->C: HANDLE  */
  GEARMAN_COMMAND_GRAB_JOB,            /* W->J: --  */
  GEARMAN_COMMAND_NO_JOB,              /* J->W: -- */
  GEARMAN_COMMAND_JOB_ASSIGN,          /* J->W: HANDLE[0]FUNC[0]ARG  */
  GEARMAN_COMMAND_WORK_STATUS,         /* W->J/C: HANDLE[0]NUMERATOR[0]DENOMINATOR */
  GEARMAN_COMMAND_WORK_COMPLETE,       /* W->J/C: HANDLE[0]RES */
  GEARMAN_COMMAND_WORK_FAIL,           /* W->J/C: HANDLE */
  GEARMAN_COMMAND_GET_STATUS,          /* C->J: HANDLE */
  GEARMAN_COMMAND_ECHO_REQ,            /* ?->J: TEXT */
  GEARMAN_COMMAND_ECHO_RES,            /* J->?: TEXT */
  GEARMAN_COMMAND_SUBMIT_JOB_BG,       /* C->J: FUNC[0]UNIQ[0]ARGS  */
  GEARMAN_COMMAND_ERROR,               /* J->?: ERRCODE[0]ERR_TEXT */
  GEARMAN_COMMAND_STATUS_RES,          /* C->J: HANDLE[0]KNOWN[0]RUNNING[0]NUM[0]DENOM */
  GEARMAN_COMMAND_SUBMIT_JOB_HIGH,     /* C->J: FUNC[0]UNIQ[0]ARGS  */
  GEARMAN_COMMAND_SET_CLIENT_ID,       /* W->J: [RANDOM_STRING_NO_WHITESPACE] */
  GEARMAN_COMMAND_CAN_DO_TIMEOUT,      /* W->J: FUNC[0]TIMEOUT */
  GEARMAN_COMMAND_ALL_YOURS,
  GEARMAN_COMMAND_WORK_EXCEPTION,
  GEARMAN_COMMAND_OPTION_REQ,
  GEARMAN_COMMAND_OPTION_RES,
  GEARMAN_COMMAND_WORK_DATA,
  GEARMAN_COMMAND_WORK_WARNING,
  GEARMAN_COMMAND_GRAB_JOB_UNIQ,
  GEARMAN_COMMAND_JOB_ASSIGN_UNIQ,
  GEARMAN_COMMAND_SUBMIT_JOB_HIGH_BG,
  GEARMAN_COMMAND_SUBMIT_JOB_LOW,
  GEARMAN_COMMAND_SUBMIT_JOB_LOW_BG,
  GEARMAN_COMMAND_SUBMIT_JOB_SCHED,
  GEARMAN_COMMAND_SUBMIT_JOB_EPOCH,
  GEARMAN_COMMAND_MAX /* Always add new commands before this. */
} gearman_command_t;

/**
 * @ingroup gearman_job
 * Priority levels for a job.
 */
typedef enum
{
  GEARMAN_JOB_PRIORITY_HIGH,
  GEARMAN_JOB_PRIORITY_NORMAL,
  GEARMAN_JOB_PRIORITY_LOW,
  GEARMAN_JOB_PRIORITY_MAX /* Always add new commands before this. */
} gearman_job_priority_t;

/**
 * @ingroup gearman_client
 * Options for gearman_client_st.
 */
typedef enum
{
  GEARMAN_CLIENT_ALLOCATED=         (1 << 0),
  GEARMAN_CLIENT_NON_BLOCKING=      (1 << 1),
  GEARMAN_CLIENT_TASK_IN_USE=       (1 << 2),
  GEARMAN_CLIENT_UNBUFFERED_RESULT= (1 << 3),
  GEARMAN_CLIENT_NO_NEW=            (1 << 4),
  GEARMAN_CLIENT_FREE_TASKS=        (1 << 5),
  GEARMAN_CLIENT_MAX=               (1 << 6)
} gearman_client_options_t;

/**
 * @ingroup gearman_worker
 * Options for gearman_worker_st.
 */
typedef enum
{
  GEARMAN_WORKER_ALLOCATED=        (1 << 0),
  GEARMAN_WORKER_NON_BLOCKING=     (1 << 1),
  GEARMAN_WORKER_PACKET_INIT=      (1 << 2),
  GEARMAN_WORKER_GRAB_JOB_IN_USE=  (1 << 3),
  GEARMAN_WORKER_PRE_SLEEP_IN_USE= (1 << 4),
  GEARMAN_WORKER_WORK_JOB_IN_USE=  (1 << 5),
  GEARMAN_WORKER_CHANGE=           (1 << 6),
  GEARMAN_WORKER_GRAB_UNIQ=        (1 << 7),
  GEARMAN_WORKER_TIMEOUT_RETURN=   (1 << 8),
  GEARMAN_WORKER_MAX=   (1 << 9)
} gearman_worker_options_t;

/**
 * @addtogroup gearman_types Types
 * @ingroup gearman_universal
 * @ingroup gearman_client
 * @ingroup gearman_worker
 * @{
 */

/* Types. */
typedef struct gearman_universal_st gearman_universal_st;
typedef struct gearman_connection_st gearman_connection_st;
typedef struct gearman_packet_st gearman_packet_st;
typedef struct gearman_command_info_st gearman_command_info_st;
typedef struct gearman_task_st gearman_task_st;
typedef struct gearman_client_st gearman_client_st;
typedef struct gearman_job_st gearman_job_st;
typedef struct gearman_worker_st gearman_worker_st;

/* Function types. */
typedef gearman_return_t (gearman_workload_fn)(gearman_task_st *task);
typedef gearman_return_t (gearman_created_fn)(gearman_task_st *task);
typedef gearman_return_t (gearman_data_fn)(gearman_task_st *task);
typedef gearman_return_t (gearman_warning_fn)(gearman_task_st *task);
typedef gearman_return_t (gearman_universal_status_fn)(gearman_task_st *task);
typedef gearman_return_t (gearman_complete_fn)(gearman_task_st *task);
typedef gearman_return_t (gearman_exception_fn)(gearman_task_st *task);
typedef gearman_return_t (gearman_fail_fn)(gearman_task_st *task);

typedef gearman_return_t (gearman_parse_server_fn)(const char *host,
                                                   in_port_t port,
                                                   void *context);

typedef void* (gearman_worker_fn)(gearman_job_st *job, void *context,
                                  size_t *result_size,
                                  gearman_return_t *ret_ptr);

/**
  @todo this is only used by the server and should be made private.
 */
typedef gearman_return_t (gearman_event_watch_fn)(gearman_connection_st *con,
                                                  short events, void *context);

typedef void* (gearman_malloc_fn)(size_t size, void *context);
typedef void (gearman_free_fn)(void *ptr, void *context);

typedef void (gearman_task_context_free_fn)(gearman_task_st *task,
                                            void *context);

typedef void (gearman_log_fn)(const char *line, gearman_verbose_t verbose,
                              void *context);

typedef void (gearman_connection_protocol_context_free_fn)(gearman_connection_st *con,
                                                           void *context);

typedef size_t (gearman_packet_pack_fn)(const gearman_packet_st *packet,
                                        gearman_connection_st *con,
                                        void *data, size_t data_size,
                                        gearman_return_t *ret_ptr);
typedef size_t (gearman_packet_unpack_fn)(gearman_packet_st *packet,
                                          gearman_connection_st *con, const void *data,
                                          size_t data_size,
                                          gearman_return_t *ret_ptr);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __GEARMAN_CONSTANTS_H__ */
