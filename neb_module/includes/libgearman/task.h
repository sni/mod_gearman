/* Gearman server and library
 * Copyright (C) 2008 Brian Aker, Eric Day
 * All rights reserved.
 *
 * Use and distribution licensed under the BSD license.  See
 * the COPYING file in the parent directory for full text.
 */

/**
 * @file
 * @brief Task Declarations
 */

#ifndef __GEARMAN_TASK_H__
#define __GEARMAN_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup gearman_task Task Declarations
 * @ingroup gearman_client
 *
 * The task functions are used to manage tasks being run by clients. They are
 * most commonly used with the client interface.
 *
 * @{
 */

/**
 * @ingroup gearman_task
 */

struct gearman_task_st
{
  struct {
    bool allocated LIBGEARMAN_BITFIELD;
    bool send_in_use LIBGEARMAN_BITFIELD;
    bool is_known LIBGEARMAN_BITFIELD;
    bool is_running LIBGEARMAN_BITFIELD;
  } options;
  enum {
    GEARMAN_TASK_STATE_NEW,
    GEARMAN_TASK_STATE_SUBMIT,
    GEARMAN_TASK_STATE_WORKLOAD,
    GEARMAN_TASK_STATE_WORK,
    GEARMAN_TASK_STATE_CREATED,
    GEARMAN_TASK_STATE_DATA,
    GEARMAN_TASK_STATE_WARNING,
    GEARMAN_TASK_STATE_STATUS,
    GEARMAN_TASK_STATE_COMPLETE,
    GEARMAN_TASK_STATE_EXCEPTION,
    GEARMAN_TASK_STATE_FAIL,
    GEARMAN_TASK_STATE_FINISHED
  } state;
  uint32_t created_id;
  uint32_t numerator;
  uint32_t denominator;
  gearman_client_st *client;
  gearman_task_st *next;
  gearman_task_st *prev;
  void *context;
  gearman_connection_st *con;
  gearman_packet_st *recv;
  gearman_packet_st send;
  char job_handle[GEARMAN_JOB_HANDLE_SIZE];
};

/**
 * Initialize a task structure.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] task Caller allocated structure, or NULL to allocate one.
 * @return On success, a pointer to the (possibly allocated) structure. On
 *  failure this will be NULL.
 */
GEARMAN_LOCAL
gearman_task_st *gearman_task_create(gearman_client_st *client,
                                     gearman_task_st *task);

/**
 * Free a task structure.
 *
 * @param[in] task Structure previously initialized with one of the
 *  gearman_client_add_task() functions.
 */
GEARMAN_API
void gearman_task_free(gearman_task_st *task);


/**
 * Get context for a task.
 */
GEARMAN_API
const void *gearman_task_context(const gearman_task_st *task);

/**
 * Set context for a task.
 */
GEARMAN_API
void gearman_task_set_context(gearman_task_st *task, void *context);

/**
 * Get function name associated with a task.
 */
GEARMAN_API
const char *gearman_task_function_name(const gearman_task_st *task);

/**
 * Get unique identifier for a task.
 */
GEARMAN_API
const char *gearman_task_unique(const gearman_task_st *task);

/**
 * Get job handle for a task.
 */
GEARMAN_API
const char *gearman_task_job_handle(const gearman_task_st *task);

/**
 * Get status on whether a task is known or not.
 */
GEARMAN_API
bool gearman_task_is_known(const gearman_task_st *task);

/**
 * Get status on whether a task is running or not.
 */
GEARMAN_API
bool gearman_task_is_running(const gearman_task_st *task);

/**
 * Get the numerator of percentage complete for a task.
 */
GEARMAN_API
uint32_t gearman_task_numerator(const gearman_task_st *task);

/**
 * Get the denominator of percentage complete for a task.
 */
GEARMAN_API
uint32_t gearman_task_denominator(const gearman_task_st *task);

/**
 * Give allocated memory to task. After this, the library will be responsible
 * for freeing the workload memory when the task is destroyed.
 */
GEARMAN_API
void gearman_task_give_workload(gearman_task_st *task, const void *workload,
                                size_t workload_size);

/**
 * Send packet workload for a task.
 */
GEARMAN_API
size_t gearman_task_send_workload(gearman_task_st *task, const void *workload,
                                  size_t workload_size,
                                  gearman_return_t *ret_ptr);

/**
 * Get result data being returned for a task.
 */
GEARMAN_API
const void *gearman_task_data(const gearman_task_st *task);

/**
 * Get result data size being returned for a task.
 */
GEARMAN_API
size_t gearman_task_data_size(const gearman_task_st *task);

/**
 * Take allocated result data from task. After this, the caller is responsible
 * for free()ing the memory.
 */
GEARMAN_API
void *gearman_task_take_data(gearman_task_st *task, size_t *data_size);

/**
 * Read result data into a buffer for a task.
 */
GEARMAN_API
size_t gearman_task_recv_data(gearman_task_st *task, void *data,
                              size_t data_size, gearman_return_t *ret_ptr);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __GEARMAN_TASK_H__ */
