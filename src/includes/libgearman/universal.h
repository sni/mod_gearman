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

#ifndef __GEARMAN_UNIVERSAL_H__
#define __GEARMAN_UNIVERSAL_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup gearman_universal
 */
struct gearman_universal_st
{
  struct {
    bool allocated LIBGEARMAN_BITFIELD; // Not Used (will remove later)
    bool dont_track_packets LIBGEARMAN_BITFIELD;
    bool non_blocking LIBGEARMAN_BITFIELD;
    bool stored_non_blocking LIBGEARMAN_BITFIELD;
  } options;
  gearman_verbose_t verbose;
  uint32_t con_count;
  uint32_t packet_count;
  uint32_t pfds_size;
  uint32_t sending;
  int last_errno;
  int timeout; // Used by poll()
  gearman_connection_st *con_list;
  gearman_packet_st *packet_list;
  struct pollfd *pfds;
  gearman_log_fn *log_fn;
  void *log_context;
  gearman_event_watch_fn *event_watch_fn;
  void *event_watch_context;
  gearman_malloc_fn *workload_malloc_fn;
  void *workload_malloc_context;
  gearman_free_fn *workload_free_fn;
  void *workload_free_context;
  char last_error[GEARMAN_MAX_ERROR_SIZE];
};

#ifdef GEARMAN_CORE


/**
 * @addtogroup gearman_universal Gearman Declarations
 *
 * This is a low level interface for gearman library instances. This is used
 * internally by both client and worker interfaces, so you probably want to
 * look there first.
 *
 * There is no locking within a single gearman_universal_st structure, so for threaded
 * applications you must either ensure isolation in the application or use
 * multiple gearman_universal_st structures (for example, one for each thread).
 *
 * @{
 */


/**
 * Initialize a gearman_universal_st structure. Always check the return value for failure.
 * Some other initialization may have failed. It is not required to memset()
 * a structure before providing it. These are for internal use only.
 *
 * @param[in] source Caller allocated structure.
 * @param[in] options gearman_universal_options_t options used to modify creation.
 * @return On success, a pointer to the (possibly allocated) structure. On
 *  failure this will be NULL.
 */
GEARMAN_INTERNAL_API
gearman_universal_st *gearman_universal_create(gearman_universal_st *source, const gearman_universal_options_t *options);

/**
 * Clone a gearman structure.
 *
 * @param[in] destination gearman_universal_st.
 * @param[in] source gearman_universal_st to clone from.
 * @return Same return as gearman_universal_create().
 */
GEARMAN_INTERNAL_API
gearman_universal_st *gearman_universal_clone(gearman_universal_st *destination, const gearman_universal_st *source);

/**
 * Free a gearman structure.
 *
 * @param[in] gearman Structure previously initialized with gearman_universal_create() or
 *  gearman_clone().
 */
GEARMAN_INTERNAL_API
void gearman_universal_free(gearman_universal_st *gearman);

/**
 * Set the error string.
 *
 * @param[in] gearman Structure previously initialized with gearman_universal_create() or
 *  gearman_clone().
 * @param[in] function Name of function the error happened in.
 * @param[in] format Format and variable argument list of message.
 */
GEARMAN_INTERNAL_API
void gearman_universal_set_error(gearman_universal_st *gearman, const char *function,
                                 const char *format, ...);

/**
 * Return an error string for last error encountered.
 *
 * @param[in] gearman Structure previously initialized with gearman_universal_create() or
 *  gearman_clone().
 * @return Pointer to a buffer in the structure that holds an error string.
 */
static inline const char *gearman_universal_error(const gearman_universal_st *gearman)
{
  if (gearman->last_error[0] == 0)
      return NULL;
  return (const char *)(gearman->last_error);
}

/**
 * Value of errno in the case of a GEARMAN_ERRNO return value.
 *
 * @param[in] gearman Structure previously initialized with gearman_universal_create() or
 *  gearman_clone().
 * @return An errno value as defined in your system errno.h file.
 */
static inline int gearman_universal_errno(const gearman_universal_st *gearman)
{
  return gearman->last_errno;
}

/**
 * Add options for a gearman structure.
 *
 * @param[in] gearman Structure previously initialized with gearman_universal_create() or
 *  gearman_clone().
 * @param[in] options Available options for gearman structures.
 */
GEARMAN_INTERNAL_API
gearman_return_t gearman_universal_set_option(gearman_universal_st *gearman, gearman_universal_options_t option, bool value);

static inline void gearman_universal_add_options(gearman_universal_st *gearman, gearman_universal_options_t options)
{
  (void)gearman_universal_set_option(gearman, options, true);
}

static inline void gearman_universal_remove_options(gearman_universal_st *gearman, gearman_universal_options_t options)
{
  (void)gearman_universal_set_option(gearman, options, false);
}

static inline bool gearman_universal_is_non_blocking(gearman_universal_st *gearman)
{
  return gearman->options.non_blocking;
}

/**
 * Get current socket I/O activity timeout value.
 *
 * @param[in] gearman Structure previously initialized with gearman_universal_create() or
 *  gearman_clone().
 * @return Timeout in milliseconds to wait for I/O activity. A negative value
 *  means an infinite timeout.
 */
GEARMAN_INTERNAL_API
int gearman_universal_timeout(gearman_universal_st *gearman);

/**
 * Set socket I/O activity timeout for connections in a Gearman structure.
 *
 * @param[in] gearman Structure previously initialized with gearman_universal_create() or
 *  gearman_clone().
 * @param[in] timeout Milliseconds to wait for I/O activity. A negative value
 *  means an infinite timeout.
 */
GEARMAN_INTERNAL_API
void gearman_universal_set_timeout(gearman_universal_st *gearman, int timeout);

/**
 * Set logging function for a gearman structure.
 *
 * @param[in] gearman Structure previously initialized with gearman_universal_create() or
 *  gearman_clone().
 * @param[in] function Function to call when there is a logging message.
 * @param[in] context Argument to pass into the callback function.
 * @param[in] verbose Verbosity level threshold. Only call function when the
 *  logging message is equal to or less verbose that this.
 */
GEARMAN_INTERNAL_API
void gearman_set_log_fn(gearman_universal_st *gearman, gearman_log_fn *function,
                        void *context, gearman_verbose_t verbose);

/**
 * Set custom I/O event callback function for a gearman structure.
 *
 * @param[in] gearman Structure previously initialized with gearman_universal_create() or
 *  gearman_clone().
 * @param[in] function Function to call when there is an I/O event.
 * @param[in] context Argument to pass into the callback function.
 */
GEARMAN_INTERNAL_API
void gearman_set_event_watch_fn(gearman_universal_st *gearman,
                                gearman_event_watch_fn *function,
                                void *context);

/**
 * Set custom memory allocation function for workloads. Normally gearman uses
 * the standard system malloc to allocate memory used with workloads. The
 * provided function will be used instead.
 *
 * @param[in] gearman Structure previously initialized with gearman_universal_create() or
 *  gearman_clone().
 * @param[in] function Memory allocation function to use instead of malloc().
 * @param[in] context Argument to pass into the callback function.
 */
GEARMAN_INTERNAL_API
void gearman_set_workload_malloc_fn(gearman_universal_st *gearman,
                                    gearman_malloc_fn *function,
                                    void *context);

/**
 * Set custom memory free function for workloads. Normally gearman uses the
 * standard system free to free memory used with workloads. The provided
 * function will be used instead.
 *
 * @param[in] gearman Structure previously initialized with gearman_universal_create() or
 *  gearman_clone().
 * @param[in] function Memory free function to use instead of free().
 * @param[in] context Argument to pass into the callback function.
 */
GEARMAN_INTERNAL_API
void gearman_set_workload_free_fn(gearman_universal_st *gearman,
                                  gearman_free_fn *function,
                                  void *context);

/**
 * Free all connections for a gearman structure.
 *
 * @param[in] gearman Structure previously initialized with gearman_universal_create() or
 *  gearman_clone().
 */
GEARMAN_INTERNAL_API
void gearman_free_all_cons(gearman_universal_st *gearman);

/**
 * Flush the send buffer for all connections.
 *
 * @param[in] gearman Structure previously initialized with gearman_universal_create() or
 *  gearman_clone().
 * @return Standard gearman return value.
 */
GEARMAN_INTERNAL_API
gearman_return_t gearman_flush_all(gearman_universal_st *gearman);

/**
 * Wait for I/O on connections.
 *
 * @param[in] gearman Structure previously initialized with gearman_universal_create() or
 *  gearman_clone().
 * @return Standard gearman return value.
 */
GEARMAN_INTERNAL_API
gearman_return_t gearman_wait(gearman_universal_st *gearman);

/**
 * Get next connection that is ready for I/O.
 *
 * @param[in] gearman Structure previously initialized with gearman_universal_create() or
 *  gearman_clone().
 * @return Connection that is ready for I/O, or NULL if there are none.
 */
GEARMAN_INTERNAL_API
gearman_connection_st *gearman_ready(gearman_universal_st *gearman);

/**
 * Test echo with all connections.
 *
 * @param[in] gearman Structure previously initialized with gearman_universal_create() or
 *  gearman_clone().
 * @param[in] workload Data to send in echo packet.
 * @param[in] workload_size Size of workload.
 * @return Standard gearman return value.
 */
GEARMAN_INTERNAL_API
gearman_return_t gearman_echo(gearman_universal_st *gearman, const void *workload,
                              size_t workload_size);

/**
 * Free all packets for a gearman structure.
 *
 * @param[in] gearman Structure previously initialized with gearman_universal_create() or
 *  gearman_clone().
 */
GEARMAN_INTERNAL_API
void gearman_free_all_packets(gearman_universal_st *gearman);

#endif /* GEARMAN_CORE */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __GEARMAN_UNIVERSAL_H__ */
