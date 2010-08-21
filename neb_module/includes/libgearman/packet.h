/* Gearman server and library
 * Copyright (C) 2008 Brian Aker, Eric Day
 * All rights reserved.
 *
 * Use and distribution licensed under the BSD license.  See
 * the COPYING file in the parent directory for full text.
 */

/**
 * @file
 * @brief Packet Declarations
 */

#ifndef __GEARMAN_PACKET_H__
#define __GEARMAN_PACKET_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup gearman_packet Packet Declarations
 * @ingroup gearman_universal
 *
 * This is a low level interface for gearman packet. This is used internally
 * internally by both client and worker interfaces (or more specifically, tasks
 * and jobs), so you probably want to look there first. This is usually used to
 * write lower level clients, workers, proxies, or your own server.
 *
 * @{
 */

enum gearman_magic_t
{
  GEARMAN_MAGIC_TEXT,
  GEARMAN_MAGIC_REQUEST,
  GEARMAN_MAGIC_RESPONSE
};

/**
 * @ingroup gearman_packet
 */
struct gearman_packet_st
{
  struct {
    bool allocated LIBGEARMAN_BITFIELD;
    bool complete LIBGEARMAN_BITFIELD;
    bool free_data LIBGEARMAN_BITFIELD;
  } options;
  enum gearman_magic_t magic;
  gearman_command_t command;
  uint8_t argc;
  size_t args_size;
  size_t data_size;
  gearman_universal_st *universal;
  gearman_packet_st *next;
  gearman_packet_st *prev;
  char *args;
  const void *data;
  char *arg[GEARMAN_MAX_COMMAND_ARGS];
  size_t arg_size[GEARMAN_MAX_COMMAND_ARGS];
  char args_buffer[GEARMAN_ARGS_BUFFER_SIZE];
};

#ifdef GEARMAN_CORE
/**
 * Command information array.
 * @ingroup gearman_constants
 */
extern GEARMAN_INTERNAL_API
gearman_command_info_st gearman_command_info_list[GEARMAN_COMMAND_MAX];


/**
 * Initialize a packet structure.
 *
 * @param[in] gearman Structure previously initialized with gearman_create() or
 *  gearman_clone().
 * @param[in] packet Caller allocated structure, or NULL to allocate one.
 * @return On success, a pointer to the (possibly allocated) structure. On
 *  failure this will be NULL.
 */
GEARMAN_INTERNAL_API
gearman_packet_st *gearman_packet_create(gearman_universal_st *gearman,
                                         gearman_packet_st *packet);

/**
 * Free a packet structure.
 *
 * @param[in] packet Structure previously initialized with
 *   gearman_packet_create() or gearman_packet_create_args().
 */
GEARMAN_INTERNAL_API
void gearman_packet_free(gearman_packet_st *packet);

/**
 * Initialize a packet with all arguments. For example:
 *
 * @code
 * void *args[3];
 * size_t args_suze[3];
 *
 * args[0]= function_name;
 * args_size[0]= strlen(function_name) + 1;
 * args[1]= unique_string;
 * args_size[1]= strlen(unique_string,) + 1;
 * args[2]= workload;
 * args_size[2]= workload_size;
 *
 * ret= gearman_packet_create_args(gearman, packet,
 *                              GEARMAN_MAGIC_REQUEST,
 *                              GEARMAN_COMMAND_SUBMIT_JOB,
 *                              args, args_size, 3);
 * @endcode
 *
 * @param[in] gearman Structure previously initialized with gearman_create() or
 *  gearman_clone().
 * @param[in] packet Pre-allocated packet to initialize with arguments.
 * @param[in] magic Magic type for packet header.
 * @param[in] command Command type for packet.
 * @param[in] args Array of arguments to add.
 * @param[in] args_size Array of sizes of each byte array in the args array.
 * @param[in] args_count Number of elements in args/args_sizes arrays.
 * @return Standard gearman return value.
 */
GEARMAN_INTERNAL_API
gearman_return_t gearman_packet_create_args(gearman_universal_st *gearman,
                                            gearman_packet_st *packet,
                                            enum gearman_magic_t magic,
                                            gearman_command_t command,
                                            const void *args[],
                                            const size_t args_size[],
                                            size_t args_count);

/**
 * Add an argument to a packet.
 */
GEARMAN_INTERNAL_API
gearman_return_t gearman_packet_create_arg(gearman_packet_st *packet,
                                           const void *arg, size_t arg_size);

/**
 * Pack header.
 */
GEARMAN_INTERNAL_API
gearman_return_t gearman_packet_pack_header(gearman_packet_st *packet);

/**
 * Unpack header.
 */
GEARMAN_INTERNAL_API
gearman_return_t gearman_packet_unpack_header(gearman_packet_st *packet);

/**
 * Pack packet into output buffer.
 */
GEARMAN_INTERNAL_API
size_t gearman_packet_pack(const gearman_packet_st *packet, gearman_connection_st *con,
                           void *data, size_t data_size,
                           gearman_return_t *ret_ptr);

/**
 * Unpack packet from input data.
 */
GEARMAN_INTERNAL_API
size_t gearman_packet_unpack(gearman_packet_st *packet, gearman_connection_st *con,
                             const void *data, size_t data_size,
                             gearman_return_t *ret_ptr);

/**
 * Give allocated memory to packet. After this, the library will be responsible
 * for freeing the workload memory when the packet is destroyed.
 */
GEARMAN_INTERNAL_API
void gearman_packet_give_data(gearman_packet_st *packet, const void *data,
                              size_t data_size);

/**
 * Take allocated data from packet. After this, the caller is responsible for
 * free()ing the memory.
 */
GEARMAN_INTERNAL_API
void *gearman_packet_take_data(gearman_packet_st *packet, size_t *data_size);

#endif /* GEARMAN_CORE */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __GEARMAN_PACKET_H__ */
