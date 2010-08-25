/* Gearman server and library
 * Copyright (C) 2008 Brian Aker, Eric Day
 * All rights reserved.
 *
 * Use and distribution licensed under the BSD license.  See
 * the COPYING file in the parent directory for full text.
 */

/**
 * @file
 * @brief Gearman Declarations
 */

#ifndef __GEARMAN_H__
#define __GEARMAN_H__

#include <inttypes.h>
#ifndef __cplusplus
#  include <stdbool.h>
#endif
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/uio.h>
#include <stdarg.h>
#include <stdlib.h>

#include <libgearman/visibility.h>
#include <libgearman/configure.h>
#include <libgearman/constants.h>
#include <libgearman/strerror.h>

// Everything above this line must be in the order specified.
#include <libgearman/core.h>
#include <libgearman/task.h>
#include <libgearman/job.h>

#include <libgearman/worker.h>
#include <libgearman/client.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup gearman Gearman Declarations
 *
 * This is a low level interface for gearman library instances. This is used
 * internally by both client and worker interfaces, so you probably want to
 * look there first. This is usually used to write lower level clients, workers,
 * proxies, or your own server.
 *
 * There is no locking within a single gearman_universal_st structure, so for threaded
 * applications you must either ensure isolation in the application or use
 * multiple gearman_universal_st structures (for example, one for each thread).
 *
 * @{
 */

/**
 * Get Gearman library version.
 *
 * @return Version string of library.
 */
GEARMAN_API
const char *gearman_version(void);

/**
 * Get bug report URL.
 *
 * @return Bug report URL string.
 */
GEARMAN_API
const char *gearman_bugreport(void);

/**
 * Get string with the name of the given verbose level.
 *
 * @param[in] verbose Verbose logging level.
 * @return String form of verbose level.
 */
GEARMAN_API
const char *gearman_verbose_name(gearman_verbose_t verbose);

/**
 * Utility function used for parsing server lists.
 *
 * @param[in] servers String containing a list of servers to parse.
 * @param[in] callback Function to call for each server that is found.
 * @param[in] context Argument to pass along with callback function.
 * @return Standard Gearman return value.
 */
GEARMAN_API
gearman_return_t gearman_parse_servers(const char *servers,
                                       gearman_parse_server_fn *callback,
                                       void *context);

/**
 * Get current socket I/O activity timeout value.
 *
 * @param[in] gearman_client_st or gearman_worker_st Structure previously initialized.
 * @return Timeout in milliseconds to wait for I/O activity. A negative value
 *  means an infinite timeout.
 * @note This is a utility macro.
 */
#define gearman_timeout(__object) ((__object)->gearman.timeout)

/**
 * Set socket I/O activity timeout for connections in a Gearman structure.
 *
 * @param[in] gearman_client_st or gearman_worker_st Structure previously initialized.
 * @param[in] timeout Milliseconds to wait for I/O activity. A negative value
 *  means an infinite timeout.
 * @note This is a utility macro.
 */
#define gearman_set_timeout(__object, __value) ((__object)->gearman.timeout)=(__value);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __GEARMAN_H__ */
