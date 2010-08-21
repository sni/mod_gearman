/* Gearman server and library
 * Copyright (C) 2008 Brian Aker, Eric Day
 * All rights reserved.
 *
 * Use and distribution licensed under the BSD license.  See
 * the COPYING file in this directory for full text.
 *
 * Implementation drawn from visibility.texi in gnulib.
 */

/**
 * @file
 * @brief Visibility Control Macros
 */

#ifndef __GEARMAN_VISIBILITY_H
#define __GEARMAN_VISIBILITY_H

/**
 * GEARMAN_API is used for the public API symbols. It either DLL imports or
 * DLL exports (or does nothing for static build).
 *
 * GEARMAN_LOCAL is used for non-api symbols.
 */

#if defined(BUILDING_LIBGEARMAN)
# if defined(HAVE_VISIBILITY)
#  define GEARMAN_API __attribute__ ((visibility("default")))
#  define GEARMAN_INTERNAL_API __attribute__ ((visibility("hidden")))
#  define GEARMAN_API_DEPRECATED __attribute__ ((deprecated,visibility("default")))
#  define GEARMAN_LOCAL  __attribute__ ((visibility("hidden")))
# elif defined (__SUNPRO_C) && (__SUNPRO_C >= 0x550)
#  define GEARMAN_API __global
#  define GEARMAN_INTERNAL_API __hidden
#  define GEARMAN_API_DEPRECATED __global
#  define GEARMAN_LOCAL __hidden
# elif defined(_MSC_VER)
#  define GEARMAN_API extern __declspec(dllexport)
#  define GEARMAN_INTERNAL_API extern __declspec(dllexport)
#  define GEARMAN_DEPRECATED_API extern __declspec(dllexport)
#  define GEARMAN_LOCAL
# endif /* defined(HAVE_VISIBILITY) */
#else  /* defined(BUILDING_LIBGEARMAN) */
# if defined(_MSC_VER)
#  define GEARMAN_API extern __declspec(dllimport)
#  define GEARMAN_INTERNAL_API extern __declspec(dllimport)
#  define GEARMAN_API_DEPRECATED extern __declspec(dllimport)
#  define GEARMAN_LOCAL
# else
#  define GEARMAN_API
#  define GEARMAN_INTERNAL_API
#  define GEARMAN_API_DEPRECATED
#  define GEARMAN_LOCAL
# endif /* defined(_MSC_VER) */
#endif /* defined(BUILDING_LIBGEARMAN) */

#endif /* __GEARMAN_VISIBILITY_H */
