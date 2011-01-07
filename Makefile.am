
###############################################################################
#
# mod_gearman - distribute checks with gearman
#
# Copyright (c) 2010 Sven Nierlein
#
###############################################################################

RPM_TOPDIR=$$(pwd)/rpm.topdir

# -g adds debug symbols, -s strips
CFLAGS=-Wall -W -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs -s -O -fPIC
# for debugging
#CFLAGS=-pedantic -Wfatal-errors -Wall -W -Wstrict-prototypes -Wshadow -Wpointer-arith -Wcast-qual -Wcast-align -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs -g3 -O -fPIC
AM_CPPFLAGS=-Iinclude
OS=`uname`

# source definitions
common_SOURCES             = common/base64.c \
                             common/crypt.c  \
                             common/rijndael.c \
                             common/gearman.c \
                             common/gearman_utils.c \
                             common/utils.c

pkglib_LIBRARIES           = mod_gearman.so
mod_gearman_so_SOURCES     = $(common_SOURCES) \
                             neb_module/gm_log.c \
                             neb_module/result_thread.c \
                             neb_module/mod_gearman.c

bin_PROGRAMS               = mod_gearman_worker \
                             send_gearman \
                             send_multi \
                             check_gearman \
                             gearman_top

mod_gearman_worker_SOURCES = $(common_SOURCES) \
                             worker/worker_logger.c   \
                             worker/worker_client.c   \
                             worker/worker.c

send_gearman_SOURCES       = $(common_SOURCES) \
                             tools/tools_logger.c \
                             tools/send_gearman.c

send_multi_SOURCES         = $(common_SOURCES) \
                             tools/tools_logger.c \
                             tools/send_multi.c

check_gearman_SOURCES      = $(common_SOURCES) \
                             tools/tools_logger.c \
                             tools/check_gearman.c

gearman_top_SOURCES        = $(common_SOURCES) \
                             tools/tools_logger.c \
                             tools/gearman_top.c
gearman_top_LDFLAGS        = -lncurses

# tests
check_PROGRAMS   = 01_utils 02_full
01_utils_SOURCES = $(common_SOURCES) t/tap.h t/tap.c worker/worker_logger.c t/01-utils.c
02_full_SOURCES  = $(common_SOURCES) t/tap.h t/tap.c worker/worker_logger.c t/02-full.c
TESTS            = 01_utils 02_full


replace_vars = sed -e 's:%CONFIG%:$(sysconfdir)/mod_gearman.conf:g' \
               -e 's:%PIDFILE%:$(localstatedir)/mod_gearman/mod_gearman_worker.pid:g' \
               -e 's:%LOGFILE%:$(localstatedir)/mod_gearman/mod_gearman_worker.log:g' \
               -e 's:%WORKERBIN%:$(bindir)/mod_gearman_worker:g' \
               -e 's:%USER%:$(user):g'

EXTRA_DIST = COPYING etc extras include \
             THANKS README README.html patches Changes worker/initscript.in

# other targets
mod_gearman.o: $(mod_gearman_so_OBJECTS) $(mod_gearman_so_DEPENDENCIES)
	if [ "$(OS)" == "Darwin" ]; then \
	    $(CXX) $(LDFLAGS) -dynamiclib -single_module -undefined dynamic_lookup $(mod_gearman_so_OBJECTS) -o $@ -lpthread -lgearman; \
	else \
	    $(CXX) $(LDFLAGS) -fPIC -shared $(mod_gearman_so_OBJECTS) -o $@ -lpthread -lgearman; \
	fi
	$(RM) mod_gearman.so

mod_gearman.conf-local:
	$(replace_vars) etc/mod_gearman.conf.in > etc/mod_gearman.conf

initscript-local:
	$(replace_vars) worker/initscript.in > worker/initscript

gearmand-init-local:
	$(replace_vars) extras/gearmand-init.in > extras/gearmand-init

all-local: mod_gearman.o initscript-local mod_gearman.conf-local gearmand-init-local
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
	$(RM) -r .deps/ Makefile.in aclocal.m4 autom4te.cache config.* configure depcomp install-sh missing *.gz

clean-local:
	$(RM) -r worker.static worker/initscript etc/mod_gearman.conf rpm.topdir *.o */*.o

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

install-exec-local: install-local-state-dir
	$(install_sh_PROGRAM) mod_gearman.o $(DESTDIR)$(pkglibdir)/mod_gearman.o
	$(install_sh_PROGRAM) -m 755 worker/initscript $(DESTDIR)$(initrddir)/mod_gearman_worker

install-local-state-dir:
	$(RM) $(DESTDIR)$(pkglibdir)/mod_gearman.so
	if [ `id -u` -eq 0 ]; then \
	    $(install_sh) -o $(user) -d $(DESTDIR)$(localstatedir)/mod_gearman/; \
    else \
	    $(install_sh) -o $(user) -d $(DESTDIR)$(localstatedir)/mod_gearman/; \
        echo "*** You should run: sudo chown $(user) $(DESTDIR)$(localstatedir)/mod_gearman/"; \
    fi

install-data-local: install-local-state-dir
	$(install_sh_PROGRAM) -m 644 extras/shared.conf            $(DESTDIR)$(datadir)/mod_gearman/shared.conf
	$(install_sh_PROGRAM) -m 644 extras/standalone_worker.conf $(DESTDIR)$(datadir)/mod_gearman/standalone_worker.conf
	@echo ""
	@echo "################################################################"
	@echo ""
	@echo " Installation completed:"
	@echo " neb module:     $(DESTDIR)$(pkglibdir)/mod_gearman.o"
	@echo ""
	@echo " worker:         $(DESTDIR)$(bindir)/mod_gearman_worker"
	@echo " init script:    $(DESTDIR)$(initrddir)/mod_gearman_worker"
	@echo ""
	@echo " check bin:      $(DESTDIR)$(bindir)/check_gearman"
	@echo " send bin:       $(DESTDIR)$(bindir)/send_gearman"
	@echo " send multi bin: $(DESTDIR)$(bindir)/send_multi"
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

test: check
	@echo "################################################################"
	@echo ""
	@echo " All tests completed successfully"
	@echo ""
	@echo "################################################################"

docs:
	$(RM) images
	ln -s /usr/share/asciidoc/images .
	asciidoc --unsafe -a toc -a toclevels=2 -a icons -a data-uri -a max-width=800 README
	chmod 644 README.html
	asciidoc --unsafe -a toc -a toclevels=2 -a icons -a data-uri -a max-width=800 QUICKSTART
	chmod 644 QUICKSTART.html

rpm: dist
	mkdir -p $(RPM_TOPDIR)/{SOURCES,BUILD,RPMS,SRPMS,SPECS}
	cp mod_gearman-$(VERSION).tar.gz $(RPM_TOPDIR)/SOURCES
	rpmbuild -ba --define "_topdir $(RPM_TOPDIR)" \
	    --buildroot=$$(pwd)/rpm.buildroot mod_gearman.spec
	mv -v $(RPM_TOPDIR)/RPMS/*/*.rpm .
	mv -v $(RPM_TOPDIR)/SRPMS/*.src.rpm .
	rm -rf $(RPM_TOPDIR) rpm.buildroot

deb:
	dpkg-buildpackage -us -uc
