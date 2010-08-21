/* Gearman server and library
 * Copyright (C) 2008 Brian Aker, Eric Day
 * All rights reserved.
 *
 * Use and distribution licensed under the BSD license.  See
 * the COPYING file in the parent directory for full text.
 */

/**
 * @file
 * @brief Struct definitions
 */

#ifndef __GEARMAN_STRUCTS_H__
#define __GEARMAN_STRUCTS_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup gearman
 */
struct gearman_st
{
  gearman_options_t options;
  gearman_verbose_t verbose;
  uint32_t con_count;
  uint32_t packet_count;
  uint32_t pfds_size;
  uint32_t sending;
  int last_errno;
  int timeout;
  gearman_con_st *con_list;
  gearman_packet_st *packet_list;
  struct pollfd *pfds;
  gearman_log_fn *log_fn;
  const void *log_context;
  gearman_event_watch_fn *event_watch_fn;
  const void *event_watch_context;
  gearman_malloc_fn *workload_malloc_fn;
  const void *workload_malloc_context;
  gearman_free_fn *workload_free_fn;
  const void *workload_free_context;
  char last_error[GEARMAN_MAX_ERROR_SIZE];
};

/**
 * @ingroup gearman_packet
 */
struct gearman_packet_st
{
  gearman_packet_options_t options;
  gearman_magic_t magic;
  gearman_command_t command;
  uint8_t argc;
  size_t args_size;
  size_t data_size;
  gearman_st *gearman;
  gearman_packet_st *next;
  gearman_packet_st *prev;
  uint8_t *args;
  const void *data;
  uint8_t *arg[GEARMAN_MAX_COMMAND_ARGS];
  size_t arg_size[GEARMAN_MAX_COMMAND_ARGS];
  uint8_t args_buffer[GEARMAN_ARGS_BUFFER_SIZE];
};

/**
 * @ingroup gearman_packet
 */
struct gearman_command_info_st
{
  const char *name;
  const uint8_t argc;
  const bool data;
};

/**
 * @ingroup gearman_con
 */
struct gearman_con_st
{
  gearman_con_options_t options;
  gearman_con_state_t state;
  gearman_con_send_state_t send_state;
  gearman_con_recv_state_t recv_state;
  in_port_t port;
  short events;
  short revents;
  int fd;
  uint32_t created_id;
  uint32_t created_id_next;
  size_t send_buffer_size;
  size_t send_data_size;
  size_t send_data_offset;
  size_t recv_buffer_size;
  size_t recv_data_size;
  size_t recv_data_offset;
  gearman_st *gearman;
  gearman_con_st *next;
  gearman_con_st *prev;
  const void *context;
  struct addrinfo *addrinfo;
  struct addrinfo *addrinfo_next;
  uint8_t *send_buffer_ptr;
  gearman_packet_st *recv_packet;
  uint8_t *recv_buffer_ptr;
  const void *protocol_context;
  gearman_con_protocol_context_free_fn *protocol_context_free_fn;
  gearman_packet_pack_fn *packet_pack_fn;
  gearman_packet_unpack_fn *packet_unpack_fn;
  gearman_packet_st packet;
  char host[NI_MAXHOST];
  uint8_t send_buffer[GEARMAN_SEND_BUFFER_SIZE];
  uint8_t recv_buffer[GEARMAN_RECV_BUFFER_SIZE];
};

/**
 * @ingroup gearman_task
 */
struct gearman_task_st
{
  gearman_task_options_t options;
  gearman_task_state_t state;
  bool is_known;
  bool is_running;
  uint32_t created_id;
  uint32_t numerator;
  uint32_t denominator;
  gearman_client_st *client;
  gearman_task_st *next;
  gearman_task_st *prev;
  const void *context;
  gearman_con_st *con;
  gearman_packet_st *recv;
  gearman_packet_st send;
  char job_handle[GEARMAN_JOB_HANDLE_SIZE];
};

/**
 * @ingroup gearman_job
 */
struct gearman_job_st
{
  gearman_job_options_t options;
  gearman_worker_st *worker;
  gearman_job_st *next;
  gearman_job_st *prev;
  gearman_con_st *con;
  gearman_packet_st assigned;
  gearman_packet_st work;
};

/**
 * @ingroup gearman_client
 */
struct gearman_client_st
{
  gearman_client_options_t options;
  gearman_client_state_t state;
  gearman_return_t do_ret;
  uint32_t new_tasks;
  uint32_t running_tasks;
  uint32_t task_count;
  size_t do_data_size;
  gearman_st *gearman;
  const void *context;
  gearman_con_st *con;
  gearman_task_st *task;
  gearman_task_st *task_list;
  gearman_task_context_free_fn *task_context_free_fn;
  void *do_data;
  gearman_workload_fn *workload_fn;
  gearman_created_fn *created_fn;
  gearman_data_fn *data_fn;
  gearman_warning_fn *warning_fn;
  gearman_status_fn *status_fn;
  gearman_complete_fn *complete_fn;
  gearman_exception_fn *exception_fn;
  gearman_fail_fn *fail_fn;
  gearman_st gearman_static;
  gearman_task_st do_task;
};

/**
 * @ingroup gearman_worker
 */
struct gearman_worker_st
{
  gearman_worker_options_t options;
  gearman_worker_state_t state;
  gearman_worker_work_state_t work_state;
  uint32_t function_count;
  uint32_t job_count;
  size_t work_result_size;
  gearman_st *gearman;
  const void *context;
  gearman_con_st *con;
  gearman_job_st *job;
  gearman_job_st *job_list;
  gearman_worker_function_st *function;
  gearman_worker_function_st *function_list;
  gearman_worker_function_st *work_function;
  void *work_result;
  gearman_st gearman_static;
  gearman_packet_st grab_job;
  gearman_packet_st pre_sleep;
  gearman_job_st work_job;
};

/**
 * @ingroup gearman_worker
 */
struct gearman_worker_function_st
{
  gearman_worker_function_options_t options;
  gearman_worker_function_st *next;
  gearman_worker_function_st *prev;
  char *function_name;
  gearman_worker_fn *worker_fn;
  const void *context;
  gearman_packet_st packet;
};

#ifdef __cplusplus
}
#endif

#endif /* __GEARMAN_STRUCTS_H__ */
