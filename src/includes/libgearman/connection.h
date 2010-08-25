/* Gearman server and library
 * Copyright (C) 2008 Brian Aker, Eric Day
 * All rights reserved.
 *
 * Use and distribution licensed under the BSD license.  See
 * the COPYING file in the parent directory for full text.
 */

/**
 * @file
 * @brief Connection Declarations
 */

#ifndef __GEARMAN_CONNECTION_H__
#define __GEARMAN_CONNECTION_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup gearman_con Connection Declarations
 * @ingroup gearman_universal
 *
 * This is a low level interface for gearman connections. This is used
 * internally by both client and worker interfaces, so you probably want to
 * look there first. This is usually used to write lower level clients, workers,
 * proxies, or your own server.
 *
 * @{
 */

/**
 * @ingroup gearman_connection
 */
struct gearman_connection_st
{
  struct {
    bool allocated LIBGEARMAN_BITFIELD;
    bool ready LIBGEARMAN_BITFIELD;
    bool packet_in_use LIBGEARMAN_BITFIELD;
    bool external_fd LIBGEARMAN_BITFIELD;
    bool ignore_lost_connection LIBGEARMAN_BITFIELD;
    bool close_after_flush LIBGEARMAN_BITFIELD;
  } options;
  enum {
    GEARMAN_CON_UNIVERSAL_ADDRINFO,
    GEARMAN_CON_UNIVERSAL_CONNECT,
    GEARMAN_CON_UNIVERSAL_CONNECTING,
    GEARMAN_CON_UNIVERSAL_CONNECTED
  } state;
  enum {
    GEARMAN_CON_SEND_STATE_NONE,
    GEARMAN_CON_SEND_UNIVERSAL_PRE_FLUSH,
    GEARMAN_CON_SEND_UNIVERSAL_FORCE_FLUSH,
    GEARMAN_CON_SEND_UNIVERSAL_FLUSH,
    GEARMAN_CON_SEND_UNIVERSAL_FLUSH_DATA
  } send_state;
  enum {
    GEARMAN_CON_RECV_UNIVERSAL_NONE,
    GEARMAN_CON_RECV_UNIVERSAL_READ,
    GEARMAN_CON_RECV_STATE_READ_DATA
  } recv_state;
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
  gearman_universal_st *universal;
  gearman_connection_st *next;
  gearman_connection_st *prev;
  void *context;
  struct addrinfo *addrinfo;
  struct addrinfo *addrinfo_next;
  char *send_buffer_ptr;
  gearman_packet_st *recv_packet;
  char *recv_buffer_ptr;
  void *protocol_context;
  gearman_connection_protocol_context_free_fn *protocol_context_free_fn;
  gearman_packet_pack_fn *packet_pack_fn;
  gearman_packet_unpack_fn *packet_unpack_fn;
  gearman_packet_st packet;
  char host[NI_MAXHOST];
  char send_buffer[GEARMAN_SEND_BUFFER_SIZE];
  char recv_buffer[GEARMAN_RECV_BUFFER_SIZE];
};

#ifdef GEARMAN_CORE

/**
 * Initialize a connection structure. Always check the return value even if
 * passing in a pre-allocated structure. Some other initialization may have
 * failed.
 *
 * @param[in] gearman Structure previously initialized with gearman_create() or
 *  gearman_clone().
 * @param[in] connection Caller allocated structure, or NULL to allocate one.
 * @return On success, a pointer to the (possibly allocated) structure. On
 *  failure this will be NULL.
 */
GEARMAN_INTERNAL_API
gearman_connection_st *gearman_connection_create(gearman_universal_st *gearman,
                                                 gearman_connection_st *connection,
                                                 gearman_connection_options_t *options);

/**
 * Create a connection structure with the given host and port.
 *
 * @param[in] gearman Structure previously initialized with gearman_create() or
 *  gearman_clone().
 * @param[in] connection Caller allocated structure, or NULL to allocate one.
 * @param[in] host Host or IP address to connect to.
 * @param[in] port Port to connect to.
 * @return On success, a pointer to the (possibly allocated) structure. On
 *  failure this will be NULL.
 */
GEARMAN_INTERNAL_API
gearman_connection_st *gearman_connection_create_args(gearman_universal_st *gearman,
                                                      gearman_connection_st *connection,
                                                      const char *host, in_port_t port);

/**
 * Clone a connection structure.
 *
 * @param[in] gearman Structure previously initialized with gearman_create() or
 *  gearman_clone().
 * @param[in] connection Caller allocated structure, or NULL to allocate one.
 * @param[in] from Structure to use as a source to clone from.
 * @return On success, a pointer to the (possibly allocated) structure. On
 *  failure this will be NULL.
 */
GEARMAN_INTERNAL_API
gearman_connection_st *gearman_connection_clone(gearman_universal_st *gearman, gearman_connection_st *src,
                                                const gearman_connection_st *from);

/**
 * Free a connection structure.
 *
 * @param[in] connection Structure previously initialized with gearman_connection_create(),
 *  gearman_connection_create_args(), or gearman_connection_clone().
 */
GEARMAN_INTERNAL_API
void gearman_connection_free(gearman_connection_st *connection);


GEARMAN_INTERNAL_API
gearman_return_t gearman_connection_set_option(gearman_connection_st *connection,
                                               gearman_connection_options_t options,
                                               bool value);


/**
 * Set host for a connection.
 */
GEARMAN_INTERNAL_API
void gearman_connection_set_host(gearman_connection_st *connection,
                                 const char *host,
                                 in_port_t port);

/**
 * Set connection to an already open file descriptor.
 */
GEARMAN_INTERNAL_API
gearman_return_t gearman_connection_set_fd(gearman_connection_st *connection, int fd);

/**
 * Get application context pointer.
 */
GEARMAN_INTERNAL_API
void *gearman_connection_context(const gearman_connection_st *connection);

/**
 * Set application context pointer.
 */
GEARMAN_INTERNAL_API
void gearman_connection_set_context(gearman_connection_st *connection, void *context);

/**
 * Connect to server.
 */
GEARMAN_INTERNAL_API
gearman_return_t gearman_connection_connect(gearman_connection_st *connection);

/**
 * Close a connection.
 */
GEARMAN_INTERNAL_API
void gearman_connection_close(gearman_connection_st *connection);

/**
 * Send packet to a connection.
 */
GEARMAN_INTERNAL_API
gearman_return_t gearman_connection_send(gearman_connection_st *connection,
                                         const gearman_packet_st *packet, bool flush);

/**
 * Send packet data to a connection.
 */
GEARMAN_INTERNAL_API
size_t gearman_connection_send_data(gearman_connection_st *connection, const void *data,
                                    size_t data_size, gearman_return_t *ret_ptr);

/**
 * Flush the send buffer.
 */
GEARMAN_INTERNAL_API
gearman_return_t gearman_connection_flush(gearman_connection_st *connection);

/**
 * Receive packet from a connection.
 */
GEARMAN_INTERNAL_API
gearman_packet_st *gearman_connection_recv(gearman_connection_st *connection,
                                           gearman_packet_st *packet,
                                           gearman_return_t *ret_ptr, bool recv_data);

/**
 * Receive packet data from a connection.
 */
GEARMAN_INTERNAL_API
size_t gearman_connection_recv_data(gearman_connection_st *connection, void *data, size_t data_size,
                                    gearman_return_t *ret_ptr);

/**
 * Read data from a connection.
 */
GEARMAN_INTERNAL_API
size_t gearman_connection_read(gearman_connection_st *connection, void *data, size_t data_size,
                               gearman_return_t *ret_ptr);

/**
 * Set events to be watched for a connection.
 */
GEARMAN_INTERNAL_API
gearman_return_t gearman_connection_set_events(gearman_connection_st *connection, short events);

/**
 * Set events that are ready for a connection. This is used with the external
 * event callbacks.
 */
GEARMAN_INTERNAL_API
gearman_return_t gearman_connection_set_revents(gearman_connection_st *connection, short revents);

/**
 * Get protocol context pointer.
 */
GEARMAN_INTERNAL_API
void *gearman_connection_protocol_context(const gearman_connection_st *connection);

/**
 * Set protocol context pointer.
 */
GEARMAN_INTERNAL_API
void gearman_connection_set_protocol_context(gearman_connection_st *connection, void *context);

/**
 * Set function to call when protocol_data should be freed.
 */
GEARMAN_INTERNAL_API
void gearman_connection_set_protocol_context_free_fn(gearman_connection_st *connection,
                                                     gearman_connection_protocol_context_free_fn *function);

/**
 * Set custom packet_pack function
 */
GEARMAN_INTERNAL_API
void gearman_connection_set_packet_pack_fn(gearman_connection_st *connection,
                                           gearman_packet_pack_fn *function);

/**
 * Set custom packet_unpack function
 */
GEARMAN_INTERNAL_API
void gearman_connection_set_packet_unpack_fn(gearman_connection_st *connection,
                                             gearman_packet_unpack_fn *function);

/** @} */

#endif /* GEARMAN_CORE */

#ifdef __cplusplus
}
#endif

#endif /* __GEARMAN_CONNECTION_H__ */
