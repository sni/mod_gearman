/* Gearman server and library
 * Copyright (C) 2010 Brian Aker, Eric Day
 * All rights reserved.
 *
 * Use and distribution licensed under the BSD license.  See
 * the COPYING file in the parent directory for full text.
 */

/**
 * @file
 * @brief Gearman Declarations
 */

#ifndef __GEARMAN_STRERROR_H__
#define __GEARMAN_STRERROR_H__

#ifdef __cplusplus
extern "C" {
#endif

GEARMAN_API
const char *gearman_strerror(gearman_return_t rc);

#ifdef __cplusplus
}
#endif

#endif /* __GEARMAN_STRERROR_H__ */
