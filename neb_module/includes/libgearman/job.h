/* Gearman server and library
 * Copyright (C) 2008 Brian Aker, Eric Day
 * All rights reserved.
 *
 * Use and distribution licensed under the BSD license.  See
 * the COPYING file in the parent directory for full text.
 */

/**
 * @file
 * @brief Job declarations
 */

#ifndef __GEARMAN_JOB_H__
#define __GEARMAN_JOB_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup gearman_job Job Management
 * @ingroup gearman_worker
 * The job functions are used to manage jobs assigned to workers. It is most
 * commonly used with the worker interface.
 * @{
 */

/**
 * Send data for a running job.
 */
GEARMAN_API
gearman_return_t gearman_job_send_data(gearman_job_st *job, const void *data,
                                       size_t data_size);

/**
 * Send warning for a running job.
 */
GEARMAN_API
gearman_return_t gearman_job_send_warning(gearman_job_st *job,
                                          const void *warning,
                                          size_t warning_size);

/**
 * Send status information for a running job.
 */
GEARMAN_API
gearman_return_t gearman_job_send_status(gearman_job_st *job,
                                         uint32_t numerator,
                                         uint32_t denominator);

/**
 * Send result and complete status for a job.
 */
GEARMAN_API
gearman_return_t gearman_job_send_complete(gearman_job_st *job,
                                           const void *result,
                                           size_t result_size);

/**
 * Send exception for a running job.
 */
GEARMAN_API
gearman_return_t gearman_job_send_exception(gearman_job_st *job,
                                            const void *exception,
                                            size_t exception_size);

/**
 * Send fail status for a job.
 */
GEARMAN_API
gearman_return_t gearman_job_send_fail(gearman_job_st *job);

/**
 * Get job handle.
 */
GEARMAN_API
char *gearman_job_handle(const gearman_job_st *job);

/**
 * Get the function name associated with a job.
 */
GEARMAN_API
char *gearman_job_function_name(const gearman_job_st *job);

/**
 * Get the unique ID associated with a job.
 */
GEARMAN_API
const char *gearman_job_unique(const gearman_job_st *job);

/**
 * Get a pointer to the workload for a job.
 */
GEARMAN_API
const void *gearman_job_workload(const gearman_job_st *job);

/**
 * Get size of the workload for a job.
 */
GEARMAN_API
size_t gearman_job_workload_size(const gearman_job_st *job);

/**
 * Take allocated workload from job. After this, the caller is responsible
 * for free()ing the memory.
 */
GEARMAN_API
void *gearman_job_take_workload(gearman_job_st *job, size_t *data_size);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __GEARMAN_JOB_H__ */
