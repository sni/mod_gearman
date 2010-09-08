
###############################################################################
#
# mod_gearman - distribute checks with gearman
#
# Copyright (c) 2010 Sven Nierlein
#
###############################################################################

AM_CPPFLAGS=-Iincludes

pkglib_LIBRARIES = mod_gearman.so
mod_gearman_so_SOURCES = base64.c crypt.c rijndael.c gearman.c utils.c logger.c result_thread.c mod_gearman.c

bin_PROGRAMS = worker
worker_SOURCES = base64.c crypt.c rijndael.c gearman.c worker_logger.c worker_client.c utils.c worker.c

# -g adds debug symbols, -s strips
CFLAGS=-Wall -W -Wstrict-prototypes -g
#mod_gearman_so_CXXFLAGS = $(CFLAGS) -fPIC -O
mod_gearman_so_CXXFLAGS = $(CFLAGS) -fPIC -O -g
mod_gearman_so_CFLAGS   = $(mod_gearman_so_CXXFLAGS)


OS=`uname`

mod_gearman.o: $(mod_gearman_so_OBJECTS) $(mod_gearman_so_DEPENDENCIES)
	if [ "$(OS)" == "Darwin" ]; then \
	    $(CXX) $(LDFLAGS) -dynamiclib -single_module -undefined dynamic_lookup $(mod_gearman_so_OBJECTS) -o $@ -lpthread -lgearman; \
	else \
	    $(CXX) $(LDFLAGS) -fPIC -shared $(mod_gearman_so_OBJECTS) -o $@ -lpthread -lgearman; \
	fi
	rm -f mod_gearman.so

mod_gearman.conf-local:
	sed 's:%LOGFILE%:$(DESTDIR)$(localstatedir)/log/mod_gearman_worker.log:g' mod_gearman.conf.in | \
	sed 's:%PIDFILE%:$(DESTDIR)$(localstatedir)/mod_gearman/mod_gearman_worker.pid:g' > mod_gearman.conf
	chmod 644 mod_gearman.conf

initscript-local:
	sed 's:%CONFIG%:$(DESTDIR)$(sysconfdir)/mod_gearman/mod_gearman.conf:g' initscript.in | \
	sed 's:%PIDFILE%:$(DESTDIR)$(localstatedir)/mod_gearman/mod_gearman_worker.pid:g' | \
	sed 's:%WORKERBIN%:$(DESTDIR)$(bindir)/worker:g' > initscript
	chmod 755 initscript

all-local: mod_gearman.o initscript-local mod_gearman.conf-local
	@echo ""
	@echo "################################################################"
	@echo ""
	@echo "  two files have been created:"
	@echo ""
	@echo "  src/mod_gearman.o"
	@echo "  src/worker"
	@echo ""
	@echo "  copy the src/mod_gearman.o to your nagios folder"
	@echo "  and at the broker= line to your config."
	@echo ""
	@echo "  read the README for configuration details"
	@echo ""
	@echo "  for static worker continue with"
	@echo "  cd src; make static-worker"
	@echo ""
	@echo "  for a normal installation continue with"
	@echo "  cd src; make install"
	@echo ""
	@echo "################################################################"

distclean-local: clean
	for file in .deps/ Makefile.in aclocal.m4 autom4te.cache config.* configure depcomp install-sh missing; do test -e $$file && rm -fr $$file; done

clean-local:
	rm -f worker.static

static-worker: worker
	$(CC) $(LDFLAGS) $(CFLAGS) -static -o worker.static $(worker_OBJECTS) -lgearman -lpthread -luuid
	@echo ""
	@echo " worker.static created."
	@echo ""

install-exec-local: 
	rm -f $(DESTDIR)$(pkglibdir)/mod_gearman.so
	$(install_sh_PROGRAM) mod_gearman.o $(DESTDIR)$(pkglibdir)/mod_gearman.o
	$(install_sh_PROGRAM) initscript $(DESTDIR)$(sysconfdir)/init.d/mod_gearman_worker

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(localstatedir)/mod_gearman/
	$(MKDIR_P) $(DESTDIR)$(localstatedir)/log/
	$(install_sh_PROGRAM) mod_gearman.conf $(DESTDIR)$(sysconfdir)/mod_gearman/mod_gearman.conf
	$(install_sh_PROGRAM) ../extras/example.conf $(DESTDIR)$(datarootdir)/mod_gearman/examples/example.conf
	@echo ""
	@echo "################################################################"
	@echo ""
	@echo " Installation completed:"
	@echo " neb module:    $(DESTDIR)$(pkglibdir)/mod_gearman.o"
	@echo " worker:        $(DESTDIR)$(bindir)/worker"
	@echo " init script:   $(DESTDIR)$(sysconfdir)/init.d/mod_gearman_worker"
	@echo " configuration: $(DESTDIR)$(sysconfdir)/mod_gearman/mod_gearman.conf"
	@echo ""
	@echo "just add the broker line to your nagios.cfg:"
	@echo "broker_module=$(DESTDIR)$(pkglibdir)/mod_gearman.o config=$(DESTDIR)$(sysconfdir)/mod_gearman/mod_gm.conf"
	@echo ""
	@echo ""
	@echo "################################################################"
