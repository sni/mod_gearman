#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

##############################################
# autoconf really does not work with 2.59 or older
AC_PREREQ([2.60])
AC_INIT([mod_gearman], [4.0.2], [sven.nierlein@consol.de])
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AC_CONFIG_SRCDIR([neb_module_naemon/mod_gearman.c],[worker/worker.c],[tools/send_gearman.c],[tools/check_gearman.c],[tools/gearman_top.c])
AC_CONFIG_HEADER([config.h])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

CFLAGS="-Wall -W -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs -O -fPIC -D_GNU_SOURCE -D_REENTRANT -D_FILE_OFFSET_BITS=64"

##############################################
AC_ARG_ENABLE(debug,--enable-debug will enable debugging symbols,[
    CFLAGS="${CFLAGS} -pedantic -Wfatal-errors -Wshadow -Wpointer-arith -Wcast-qual -Wcast-align -g3"
    ],
    # do not strip binarys, this is done on packaging automatically and prevents debug packages to build properly
    #CFLAGS="${CFLAGS} -s"
    CFLAGS="${CFLAGS} -g3"
)

##############################################
# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_RANLIB
AM_PROG_AR

##############################################
# gearman wants this
AC_C_INLINE
AC_FUNC_MALLOC
AC_FUNC_FORK
AC_HEADER_STDBOOL

##############################################
# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create])

##############################################
# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h pthread.h arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h stddef.h sys/socket.h sys/time.h sys/timeb.h syslog.h],,AC_MSG_ERROR([Compiling Mod-Gearman requires standard unix headers files]))
AC_CHECK_HEADERS([ltdl.h],,AC_MSG_ERROR([Compiling Mod-Gearman requires ltdl.h]))
AC_CHECK_HEADERS([curses.h],,AC_MSG_ERROR([Compiling Mod-Gearman requires curses.h]))

AC_ARG_WITH(gearman,
 [  --with-gearman=DIR Specify the path to your gearman library],
 [
    found=0
    for d in libgearman/.libs lib lib64 .; do
      test -e "$withval/$d/libgearman.so" && LDFLAGS="${LDFLAGS} -L$withval/$d" && CFLAGS="${CFLAGS} -L$withval/$d" && found=1 && break;
    done
    test $found -eq 1 || { echo 'did not find libgearman.so under your specified path!' && exit 1; }
    echo "found libgearman.so in $withval/$d/"
    found=0
    for d in libgearman-1.0 include .; do
      test -e "$withval/$d/libgearman-1.0/gearman.h" && CFLAGS="${CFLAGS} -I$withval/$d" && found=1 && break;
    done
    test $found -eq 1 || { echo 'did not find libgearman-1.0/gearman.h under your specified path!' && exit 1; }
    echo "found gearman.h in $withval/$d/"
 ],
 [
    PKG_CHECK_MODULES([gearmand], [gearmand >= 1.1.12])
 ]
)

##############################################
# Checks for additional libraries.
AC_CHECK_LIB([gearman], [gearman_client_create],,AC_MSG_ERROR([Compiling Mod-Gearman requires libgearman. You may specify the path by --with-gearman=path...]))

##############################################
AC_ARG_WITH(naemon,
 [  --with-naemon=DIR Specify the path to libnaemon],
 [
    found=0
    for d in .; do
      test -e "$withval/$d/libnaemon.la" && LDFLAGS="${LDFLAGS} -L$withval/$d" && CFLAGS="${CFLAGS} -L$withval/$d" && found=1 && break;
    done
    test $found -eq 1 || { echo 'did not find libnaemon.la under your specified path!' && exit 1; }
    echo "found libnaemon.la in $withval/$d/"
    found=0
    for d in .; do
      test -e "$withval/$d/src/naemon/naemon.h"  && CFLAGS="${CFLAGS} -I$withval/$d -I$withval/$d/src" && found=1 && break;
    done
    echo "found naemon.h in $withval/$d/"
    test $found -eq 1 || { echo 'did not find naemon/naemon.h under your specified path!' && exit 1; }
    CPPFLAGS=`PKG_CONFIG_PATH="$withval/$d/" pkg-config --cflags naemon`
 ],
 [
    PKG_CHECK_MODULES([naemon], [naemon >= 1.2])
    CPPFLAGS=`pkg-config --cflags naemon`
    AC_CHECK_HEADERS([naemon/naemon.h],,AC_MSG_ERROR([Compiling Mod-Gearman requires naemon.h (hint: install naemon-dev pkg)]))
 ]
)

##############################################
# Determine the system init.d directory
AC_ARG_WITH([init-dir],
[AS_HELP_STRING([--with-init-dir],
[specify the system init script directory @<:@default="${sysconfdir}/init.d"@:>@])],
[], [with_init_dir="${sysconfdir}/init.d"])
initrddir="$with_init_dir"
AC_SUBST(initrddir)

##############################################
# Determine the user
AC_ARG_WITH([user],
[AS_HELP_STRING([--with-user],
[specify the user @<:@default="naemon"@:>@])],
[], [with_user="naemon"])
user="$with_user"
AC_SUBST(user)

##############################################
USEPERL=no;
AC_ARG_ENABLE(embedded-perl,--enable-embedded-perl will enable embedded Perl interpreter,[
    USEPERL=$enableval
    ]
    ,USEPERL=no)

dnl Is embedded Perl being compiled in?
AM_CONDITIONAL(USEPERL, test "$USEPERL" = "yes")
if test "$USEPERL" = "yes"; then
    tmp=$LIBS
    AC_CHECK_LIB([perl],[perl_alloc],,AC_MSG_ERROR([Compiling Mod-Gearman with embedded Perl requires libperl.]),[`perl -MExtUtils::Embed -e ccopts -e ldopts`])
    # save libs, as we do not want to add -lperl to all LIBS
    LIBS=$tmp
    AC_DEFINE_UNQUOTED(EMBEDDEDPERL,,[Is embedded Perl being compiled in?])
    echo "creating common/perlxsi.c"
    perl -MExtUtils::Embed -e xsinit -- -o common/perlxsi.c
    echo "Embedded Perl interpreter will be compiled in..."
    PERLLIBS="`perl -MExtUtils::Embed -e ccopts -e ldopts | tr -d '\n'`"
    AC_SUBST(PERLLIBS)
fi

##############################################
AM_CONDITIONAL(USEBSD, test "$(uname)" = "FreeBSD")

##############################################
# Check some functions
AC_CHECK_FUNCS([gettimeofday strsep strtok strdup strchr strstr strtoul alarm gethostname memset strcspn strerror atexit gethostbyname socket dup2 localtime_r memmove strpbrk])
AC_PROG_LN_S

##############################################
# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_PID_T

##############################################
# add a "make install" target
AC_PROG_INSTALL

##############################################
# write out files
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
