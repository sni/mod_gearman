/* Gearman server and library
 * Copyright (C) 2008 Brian Aker, Eric Day
 * All rights reserved.
 *
 * Use and distribution licensed under the BSD license.  See
 * the COPYING file in the parent directory for full text.
 */

/**
 * @file
 * @brief Task declarations
 */

#ifndef __GEARMAN_TASK_H__
#define __GEARMAN_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup gearman_task Task Management
 * @ingroup gearman_client
 * The task functions are used to manage tasks being run by clients. They are
 * most commonly used with the client interface.
 * @{
 */

/**
 * Get context for a task.
 */
GEARMAN_API
void *gearman_task_context(const gearman_task_st *task);

/**
 * Set context for a task.
 */
GEARMAN_API
void gearman_task_set_context(gearman_task_st *task, const void *context);

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
