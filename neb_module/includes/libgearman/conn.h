/* Gearman server and library
 * Copyright (C) 2008 Brian Aker, Eric Day
 * All rights reserved.
 *
 * Use and distribution licensed under the BSD license.  See
 * the COPYING file in the parent directory for full text.
 */

/**
 * @file
 * @brief Connection declarations
 */

#ifndef __GEARMAN_CON_H__
#define __GEARMAN_CON_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup gearman_con Connection Handling
 * This is a low level interface for gearman connections. This is used
 * internally by both client and worker interfaces, so you probably want to
 * look there first. This is usually used to write lower level clients, workers,
 * proxies, or your own server.
 * @{
 */

/**
 * Set host for a connection.
 */
GEARMAN_API
void gearman_con_set_host(gearman_con_st *con, const char *host);

/**
 * Set port for a connection.
 */
GEARMAN_API
void gearman_con_set_port(gearman_con_st *con, in_port_t port);

/**
 * Get options for a connection.
 */
GEARMAN_API
gearman_con_options_t gearman_con_options(const gearman_con_st *con);

/**
 * Set options for a connection.
 */
GEARMAN_API
void gearman_con_set_options(gearman_con_st *con,
                             gearman_con_options_t options);

/**
 * Add options for a connection.
 */
GEARMAN_API
void gearman_con_add_options(gearman_con_st *con,
                             gearman_con_options_t options);

/**
 * Remove options for a connection.
 */
GEARMAN_API
void gearman_con_remove_options(gearman_con_st *con,
                                gearman_con_options_t options);

/**
 * Set connection to an already open file descriptor.
 */
GEARMAN_API
gearman_return_t gearman_con_set_fd(gearman_con_st *con, int fd);

/**
 * Get application context pointer.
 */
GEARMAN_API
void *gearman_con_context(const gearman_con_st *con);

/**
 * Set application context pointer.
 */
GEARMAN_API
void gearman_con_set_context(gearman_con_st *con, const void *context);

/**
 * Connect to server.
 */
GEARMAN_API
gearman_return_t gearman_con_connect(gearman_con_st *con);

/**
 * Close a connection.
 */
GEARMAN_API
void gearman_con_close(gearman_con_st *con);

/**
 * Clear address info, freeing structs if needed.
 */
GEARMAN_API
void gearman_con_reset_addrinfo(gearman_con_st *con);

/**
 * Send packet to a connection.
 */
GEARMAN_API
gearman_return_t gearman_con_send(gearman_con_st *con,
                                  const gearman_packet_st *packet, bool flush);

/**
 * Send packet data to a connection.
 */
GEARMAN_API
size_t gearman_con_send_data(gearman_con_st *con, const void *data,
                             size_t data_size, gearman_return_t *ret_ptr);

/**
 * Flush the send buffer.
 */
GEARMAN_API
gearman_return_t gearman_con_flush(gearman_con_st *con);

/**
 * Receive packet from a connection.
 */
GEARMAN_API
gearman_packet_st *gearman_con_recv(gearman_con_st *con,
                                    gearman_packet_st *packet,
                                    gearman_return_t *ret_ptr, bool recv_data);

/**
 * Receive packet data from a connection.
 */
GEARMAN_API
size_t gearman_con_recv_data(gearman_con_st *con, void *data, size_t data_size,
                             gearman_return_t *ret_ptr);

/**
 * Read data from a connection.
 */
GEARMAN_API
size_t gearman_con_read(gearman_con_st *con, void *data, size_t data_size,
                        gearman_return_t *ret_ptr);

/**
 * Set events to be watched for a connection.
 */
GEARMAN_API
gearman_return_t gearman_con_set_events(gearman_con_st *con, short events);

/**
 * Set events that are ready for a connection. This is used with the external
 * event callbacks.
 */
GEARMAN_API
gearman_return_t gearman_con_set_revents(gearman_con_st *con, short revents);

/**
 * Get protocol context pointer.
 */
GEARMAN_API
void *gearman_con_protocol_context(const gearman_con_st *con);

/**
 * Set protocol context pointer.
 */
GEARMAN_API
void gearman_con_set_protocol_context(gearman_con_st *con, const void *context);

/**
 * Set function to call when protocol_data should be freed.
 */
GEARMAN_API
void gearman_con_set_protocol_context_free_fn(gearman_con_st *con,
                                gearman_con_protocol_context_free_fn *function);

/**
 * Set custom packet_pack function
 */
GEARMAN_API
void gearman_con_set_packet_pack_fn(gearman_con_st *con,
                                    gearman_packet_pack_fn *function);

/**
 * Set custom packet_unpack function
 */
GEARMAN_API
void gearman_con_set_packet_unpack_fn(gearman_con_st *con,
                                      gearman_packet_unpack_fn *function);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __GEARMAN_CON_H__ */
