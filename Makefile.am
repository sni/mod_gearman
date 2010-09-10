
###############################################################################
#
# mod_gearman - distribute checks with gearman
#
# Copyright (c) 2010 Sven Nierlein
#
###############################################################################

AM_CPPFLAGS=-Iinclude

common_SOURCES             = common/base64.c \
                             common/crypt.c  \
                             common/rijndael.c \
                             common/gearman.c \
                             common/utils.c

pkglib_LIBRARIES           = mod_gearman.so
mod_gearman_so_SOURCES     = $(common_SOURCES) \
                             neb_module/logger.c \
                             neb_module/result_thread.c \
                             neb_module/mod_gearman.c

bin_PROGRAMS               = mod_gearman_worker
mod_gearman_worker_SOURCES = $(common_SOURCES) \
                             worker/worker_logger.c   \
                             worker/worker_client.c   \
                             worker/worker.c

# tests
check_PROGRAMS   = 01_utils
01_utils_SOURCES = $(common_SOURCES) t/tap.h t/tap.c worker/worker_logger.c t/01-utils.c
01_utils_CFLAGS  = -Wall --pedantic -std=c99 -fPIC -O -g
TESTS            = 01_utils


# -g adds debug symbols, -s strips
CFLAGS=-Wall -W -Wstrict-prototypes -g
#mod_gearman_so_CXXFLAGS = $(CFLAGS) -fPIC -O
mod_gearman_so_CXXFLAGS = $(CFLAGS) -fPIC -O -g
mod_gearman_so_CFLAGS   = $(mod_gearman_so_CXXFLAGS)

replace_vars = sed 's:%CONFIG%:$(DESTDIR)$(sysconfdir)/mod_gearman.conf:g' | \
               sed 's:%PIDFILE%:$(DESTDIR)$(localstatedir)/mod_gearman/mod_gearman_worker.pid:g' | \
               sed 's:%WORKERBIN%:$(DESTDIR)$(bindir)/mod_gearman_worker:g' | \
               sed 's:%USER%:$(user):g' | \
               sed 's:%LOGFILE%:$(DESTDIR)$(localstatedir)/log/mod_gearman_worker.log:g'



OS=`uname`

mod_gearman.o: $(mod_gearman_so_OBJECTS) $(mod_gearman_so_DEPENDENCIES)
	if [ "$(OS)" == "Darwin" ]; then \
	    $(CXX) $(LDFLAGS) -dynamiclib -single_module -undefined dynamic_lookup $(mod_gearman_so_OBJECTS) -o $@ -lpthread -lgearman; \
	else \
	    $(CXX) $(LDFLAGS) -fPIC -shared $(mod_gearman_so_OBJECTS) -o $@ -lpthread -lgearman; \
	fi
	rm -f mod_gearman.so

mod_gearman.conf-local:
	cat etc/mod_gearman.conf.in | $(replace_vars) > etc/mod_gearman.conf
	chmod 644 etc/mod_gearman.conf

initscript-local:
	cat worker/initscript.in | $(replace_vars) > worker/initscript
	chmod 755 worker/initscript

all-local: mod_gearman.o initscript-local mod_gearman.conf-local
	@echo ""
	@echo "################################################################"
	@echo ""
	@echo "  the following files have been created:"
	@echo ""
	@echo "  mod_gearman.o"
	@echo "  mod_gearman_worker"
	@echo "  etc/mod_gearman.conf"
	@echo "  worker/initscript"
	@echo ""
	@echo "  read the README for configuration details"
	@echo ""
	@echo "  for static worker continue with"
	@echo "  make worker.static"
	@echo ""
	@echo "  for a normal installation continue with"
	@echo "  make install"
	@echo ""
	@echo "################################################################"

distclean-local: clean
	for file in .deps/ Makefile.in aclocal.m4 autom4te.cache config.* configure depcomp install-sh missing; do test -e $$file && rm -fr $$file; done

clean-local:
	rm -f worker.static worker/initscript etc/mod_gearman.conf
	rm -f *.o */*.o

worker.static: worker
	@echo "################################################################"
	@echo ""
	@echo " if the static compiler complains about 'cannot find -lgearman', you have to"
	@echo " compile libgearman with -static"
	@echo ""
	@echo "################################################################"
	cd worker && $(CC) $(LDFLAGS) $(CFLAGS) -static -o worker.static $(worker_OBJECTS) -lgearman -lpthread -luuid
	@echo ""
	@echo " worker.static created."
	@echo ""

install-exec-local:
	rm -f $(DESTDIR)$(pkglibdir)/mod_gearman.so
	$(install_sh_PROGRAM) mod_gearman.o $(DESTDIR)$(pkglibdir)/mod_gearman.o
	$(install_sh_PROGRAM) worker/initscript $(DESTDIR)$(initrddir)/mod_gearman_worker

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(localstatedir)/mod_gearman/
	$(MKDIR_P) $(DESTDIR)$(localstatedir)/log/
	$(install_sh_PROGRAM) extras/example.conf $(DESTDIR)$(datarootdir)/examples/example.conf
	@echo ""
	@echo "################################################################"
	@echo ""
	@echo " Installation completed:"
	@echo " neb module:    $(DESTDIR)$(pkglibdir)/mod_gearman.o"
	@echo " worker:        $(DESTDIR)$(bindir)/mod_gearman_worker"
	@echo " init script:   $(DESTDIR)$(initrddir)/mod_gearman_worker"
	@echo ""
	@echo "just add the broker line to your nagios.cfg:"
	@echo "broker_module=$(DESTDIR)$(pkglibdir)/mod_gearman.o config=$(DESTDIR)$(sysconfdir)/mod_gearman.conf"
	@echo ""
	@echo " install default config with"
	@echo ""
	@echo " make install-config"
	@echo ""
	@echo "################################################################"


install-config:
	$(install_sh_PROGRAM) etc/mod_gearman.conf $(DESTDIR)$(sysconfdir)/mod_gearman.conf
	@echo "################################################################"
	@echo ""
	@echo " configuration: $(DESTDIR)$(sysconfdir)/mod_gearman.conf"
	@echo ""
	@echo "################################################################"
