#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

##############################################
AC_PREREQ([2.65])
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
# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create])

##############################################
# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h pthread.h])

##############################################
# Check some functions
AC_CHECK_FUNCS([neb_register_callback])
AC_CHECK_FUNCS([neb_deregister_callback])
AC_CHECK_FUNCS([add_check_result_to_list])
AC_CHECK_FUNCS([find_host])
AC_CHECK_FUNCS([find_service])

##############################################
# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

##############################################
# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([strdup strchr])

##############################################
# add a "make install" target
# AC_PROG_INSTALL

##############################################
# write out files
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
