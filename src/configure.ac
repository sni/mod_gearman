#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

##############################################
AC_PREREQ([2.61])
AC_INIT([mod_gearman], [0.1], [sven@nierlein.de])
AM_INIT_AUTOMAKE([-Wall foreign])
AC_CONFIG_SRCDIR([mod_gearman.c])
AC_CONFIG_HEADER([config.h])

##############################################
# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_RANLIB

##############################################
# gearman wants this
AC_C_INLINE
AC_FUNC_MALLOC
AC_HEADER_STDBOOL

##############################################
# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create])

##############################################
# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h pthread.h arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h stddef.h sys/socket.h sys/time.h sys/timeb.h syslog.h],,AC_MSG_ERROR([Compiling mod_gearman requires standard unix headers files]))
AC_CHECK_HEADERS([ltdl.h],,AC_MSG_ERROR([Compiling mod_gearman requires ltdl.h]))

AC_ARG_WITH(gearman,
 [  --with-gearman=DIR Specify the path to you gearman library],
 [  LDFLAGS="$LDFLAGS -L$withval/lib"
    CFLAGS="$CFLAGS -I$withval/include"
 ])

##############################################
# Check some functions
AC_CHECK_FUNCS([gettimeofday strdup strchr])

##############################################
# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

##############################################
# Checks for additional libraries.
AC_CHECK_LIB([gearman], [gearman_client_create],,AC_MSG_ERROR([Compiling mod_gearman requires libgearman]))

##############################################
# add a "make install" target
# AC_PROG_INSTALL

##############################################
# write out files
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
