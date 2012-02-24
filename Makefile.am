# vim:ft=automake noexpandtab
###############################################################################
#
# mod_gearman - distribute checks with gearman
#
# Copyright (c) 2010 Sven Nierlein
#
###############################################################################

RPM_TOPDIR=$$(pwd)/rpm.topdir
DOS2UNIX=$(shell which dos2unix || which fromdos)

.PHONY: docs

AM_CPPFLAGS=-Iinclude
OS=$(shell uname)

if USEPERL
P1FILE         = worker/mod_gearman_p1.pl
PERLLIB        = @PERLLIBS@
perl_objects   = common/perlxsi.o common/epn_utils.o
install_p1file = install_p1file
EXTRA_mod_gearman_worker_SOURCES=common/perlxsi.c common/epn_utils.c
01_utils_LDADD             = $(PERLLIB) common/perlxsi.o common/epn_utils.o
02_full_LDADD              = $(PERLLIB) common/perlxsi.o common/epn_utils.o
03_exec_LDADD              = $(PERLLIB) common/perlxsi.o common/epn_utils.o
06_exec_LDADD              = $(PERLLIB) common/perlxsi.o common/epn_utils.o
07_epn_LDADD               = $(PERLLIB) common/perlxsi.o common/epn_utils.o
else
PERLLIB        =
P1FILE         =
install_p1file =
perl_objects   =
endif

# source definitions
common_SOURCES             = common/base64.c \
                             common/gm_crypt.c  \
                             common/rijndael.c \
                             common/gearman.c \
                             common/gearman_utils.c \
                             common/utils.c \
                             common/md5.c

common_check_SOURCES       = common/check_utils.c \
                             common/popenRWE.c \
                             worker/worker_client.c

pkglib_LIBRARIES           = mod_gearman.so
mod_gearman_so_SOURCES     = $(common_SOURCES) \
                             neb_module/result_thread.c \
                             neb_module/mod_gearman.c

bin_PROGRAMS               = mod_gearman_worker \
                             send_gearman \
                             send_multi \
                             check_gearman \
                             gearman_top

mod_gearman_worker_SOURCES = $(common_SOURCES) \
                             $(common_check_SOURCES) \
                             worker/worker.c

send_gearman_SOURCES       = $(common_SOURCES) \
                             tools/send_gearman.c

send_multi_SOURCES         = $(common_SOURCES) \
                             tools/send_multi.c

check_gearman_SOURCES      = $(common_SOURCES) \
                             tools/check_gearman.c

gearman_top_SOURCES        = $(common_SOURCES) \
                             tools/gearman_top.c
gearman_top_LDADD          = -lncurses

# tests
check_PROGRAMS   = 01_utils 02_full 03_exec 04_log 05_neb 06_exec 07_epn
01_utils_SOURCES = $(common_SOURCES) t/tap.h t/tap.c t/01-utils.c $(common_check_SOURCES)
02_full_SOURCES  = $(common_SOURCES) t/tap.h t/tap.c t/02-full.c $(common_check_SOURCES)
03_exec_SOURCES  = $(common_SOURCES) t/tap.h t/tap.c t/03-exec_checks.c $(common_check_SOURCES)
04_log_SOURCES   = $(common_SOURCES) t/tap.h t/tap.c t/04-log.c
05_neb_SOURCES   = $(common_SOURCES) t/tap.h t/tap.c t/05-neb.c
if USEBSD
05_neb_LDADD=
else
05_neb_LDADD=-ldl
endif
05_neb_LDFLAGS   = --export-dynamic -rdynamic
07_epn_SOURCES   = $(common_SOURCES) t/tap.h t/tap.c t/07-epn.c $(common_check_SOURCES)
# only used for performance tests
06_exec_SOURCES  = $(common_SOURCES) t/06-execvp_vs_popen.c $(common_check_SOURCES)
TESTS            = $(check_PROGRAMS)


GEARMANDS=/usr/sbin/gearmand /opt/sbin/gearmand
replace_vars = sed -e 's:%CONFIG_WORKER%:$(sysconfdir)/mod_gearman_worker.conf:g' \
               -e 's:%PIDFILE%:$(localstatedir)/mod_gearman/mod_gearman_worker.pid:g' \
               -e 's:%LOGFILE_NEB%:$(localstatedir)/log/mod_gearman/mod_gearman_neb.log:g' \
               -e 's:%LOGFILE_WORKER%:$(localstatedir)/log/mod_gearman/mod_gearman_worker.log:g' \
               -e 's:%P1FILE%:$(datadir)/mod_gearman/mod_gearman_p1.pl:g' \
               -e 's:%GPIDFILE%:$(localstatedir)/mod_gearman/gearmand.pid:g' \
               -e 's:%GLOGFILE%:$(localstatedir)/mod_gearman/gearmand.log:g' \
               -e 's:%WORKERBIN%:$(bindir)/mod_gearman_worker:g' \
               -e 's:%USER%:$(user):g'

EXTRA_DIST = COPYING etc/*.in extras include \
             THANKS README docs/README.html Changes worker/initscript.in \
             docs/QUICKSTART docs/QUICKSTART.html support/mod_gearman.spec \
             t/data/test1.key t/data/test2.key t/data/test3.key t/data/send_multi.txt \
             t/rc t/both t/killer t/sleep t/crit.pl t/ok.pl t/noepn.pl t/fail.pl \
             worker/mod_gearman_p1.pl t/test_all.pl t/valgrind_suppress.cfg

# other targets
mod_gearman.o: $(mod_gearman_so_OBJECTS) $(mod_gearman_so_DEPENDENCIES)
	@echo '    $$(CC) $<'
	@if [ "$(OS)" = "Darwin" ]; then \
	    $(CXX) $(LDFLAGS) -dynamiclib -single_module -undefined dynamic_lookup $(mod_gearman_so_OBJECTS) -o $@ -lpthread -lgearman; \
	else \
	    $(CXX) $(LDFLAGS) -fPIC -shared $(mod_gearman_so_OBJECTS) -o $@ -lpthread -lgearman; \
	fi
	@$(RM) mod_gearman.so

common/perlxsi.o:
	@echo '    $$(CC) $<'
	@depbase=`echo $@ | sed 's|[^/]*$$|$(DEPDIR)/&|;s|\.o$$||'`;\
	$(COMPILE) `perl -MExtUtils::Embed -e ccopts` -MT $@ -MD -MP -MF $$depbase.Tpo -c -o $@ common/perlxsi.c &&\
	$(am__mv) $$depbase.Tpo $$depbase.Po

common/epn_utils.o:
	@echo '    $$(CC) $<'
	@depbase=`echo $@ | sed 's|[^/]*$$|$(DEPDIR)/&|;s|\.o$$||'`;\
	$(COMPILE) `perl -MExtUtils::Embed -e ccopts` -MT $@ -MD -MP -MF $$depbase.Tpo -c -o $@ common/epn_utils.c &&\
	$(am__mv) $$depbase.Tpo $$depbase.Po

mod_gearman_neb.conf-local:
	@$(replace_vars) etc/mod_gearman_neb.conf.in > etc/mod_gearman_neb.conf

mod_gearman_worker.conf-local:
	@$(replace_vars) etc/mod_gearman_worker.conf.in > etc/mod_gearman_worker.conf

initscript-local:
	@$(replace_vars) worker/initscript.in > worker/initscript

gearmand-init-local:
	@$(replace_vars) extras/gearmand-init.in > extras/gearmand-init
	@if [ ! -z $$(which gearmand 2>/dev/null) ]; then \
	    sed -i -e "s:%GEARMAND%:$$(which gearmand 2>/dev/null):g" extras/gearmand-init; \
	fi
	@for gearm in $(GEARMANDS); do \
  	    test -f $$gearm && sed -i -e "s:%GEARMAND%:$$gearm:g" extras/gearmand-init ; \
	done; echo done
	@sed -i -e 's:%GEARMAND%:/usr/sbin/gearmand:g' extras/gearmand-init
	@chmod 755 extras/gearmand-init

all-local: mod_gearman.o initscript-local mod_gearman_neb.conf-local mod_gearman_worker.conf-local gearmand-init-local
	@echo ""
	@echo "################################################################"
	@echo ""
	@echo "  the following files have been created:"
	@echo ""
	@echo "  mod_gearman.o"
	@echo "  mod_gearman_worker"
	@echo "  worker/initscript"
	@echo "  etc/mod_gearman_neb.conf"
	@echo "  etc/mod_gearman_worker.conf"
	@echo "  $(P1FILE)"
	@echo ""
	@echo "  read the README for configuration details"
	@echo ""
	@echo "  for a normal installation continue with"
	@echo "  make install"
	@echo ""
	@echo "################################################################"

distclean-local: clean
	$(RM) -rf .deps/ Makefile.in aclocal.m4 autom4te.cache config.* configure depcomp install-sh missing *.gz

clean-local:
	$(RM) -f worker.static worker/initscript etc/mod_gearman.conf rpm.topdir *.o */*.o extras/gearmand-init etc/mod_gearman_neb.conf etc/mod_gearman_worker.conf

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
	$(install_sh_PROGRAM) -m 755 extras/gearmand-init $(DESTDIR)$(initrddir)/gearmand

install-local-state-dir:
	$(RM) $(DESTDIR)$(pkglibdir)/mod_gearman.so
	$(install_sh_PROGRAM) -d $(DESTDIR)$(localstatedir)/mod_gearman/

install_p1file:
	$(install_sh_PROGRAM) -m 755 $(P1FILE)  $(DESTDIR)$(datadir)/mod_gearman/mod_gearman_p1.pl
	@echo "################################################################"
	@echo " Installation completed:"
	@echo " p1file:         $(DESTDIR)$(datadir)/mod_gearman/mod_gearman_p1.pl"
	@echo "################################################################"

install-data-local: install-local-state-dir $(install_p1file)
	$(install_sh_PROGRAM) -d $(DESTDIR)$(localstatedir)/log/mod_gearman
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
	@echo "broker_module=$(DESTDIR)$(pkglibdir)/mod_gearman.o config=$(DESTDIR)$(sysconfdir)/mod_gearman_neb.conf"
	@echo ""
	@if [ -e $(DESTDIR)$(sysconfdir)/mod_gearman_neb.conf -o -e $(DESTDIR)$(sysconfdir)/mod_gearman_worker.conf ]; then \
	    echo " install default config with"; \
	    echo ""; \
	    echo " make install-config"; \
	    echo ""; \
	else \
	    $(install_sh_PROGRAM) etc/mod_gearman_neb.conf $(DESTDIR)$(sysconfdir)/mod_gearman_neb.conf; \
	    $(install_sh_PROGRAM) etc/mod_gearman_worker.conf $(DESTDIR)$(sysconfdir)/mod_gearman_worker.conf; \
	fi
	@echo " configuration:"
	@echo "         neb module: $(DESTDIR)$(sysconfdir)/mod_gearman_neb.conf"
	@echo "         worker:     $(DESTDIR)$(sysconfdir)/mod_gearman_worker.conf"
	@echo ""
	@echo "################################################################"


install-config:
	$(install_sh_PROGRAM) etc/mod_gearman_neb.conf $(DESTDIR)$(sysconfdir)/mod_gearman_neb.conf
	$(install_sh_PROGRAM) etc/mod_gearman_worker.conf $(DESTDIR)$(sysconfdir)/mod_gearman_worker.conf
	@echo "################################################################"
	@echo ""
	@echo " configuration:"
	@echo "         neb module: $(DESTDIR)$(sysconfdir)/mod_gearman_neb.conf"
	@echo "         worker:     $(DESTDIR)$(sysconfdir)/mod_gearman_worker.conf"
	@echo ""
	@echo "################################################################"

test: check
	@echo "################################################################"
	@echo ""
	@echo " All tests completed successfully"
	@echo ""
	@echo "################################################################"

fulltest:
	./t/test_all.pl
	@echo "################################################################"
	@echo ""
	@echo " Full tests completed successfully"
	@echo ""
	@echo "################################################################"

docs:
	@if [ -z "$(DOS2UNIX)" ]; then \
	    printf "\n\n**** please install dos2unix or tofrodos package ****\n\n"; \
	    exit 1; \
	fi
	@if [ `type doxygen > /dev/null 2>&1` ]; then \
	    doxygen Doxyfile; \
	else \
	    printf "\n\n**** please install doxygen to generate doxygen docs ****\n\n"; \
	fi;
	$(RM) docs/images
	cd docs && ln -s /usr/share/asciidoc/images .
	cp -p README.asciidoc docs/README && cd docs && asciidoc --unsafe -a toc -a toclevels=2 -a max-width=800 README
	chmod 644 docs/README.html
	$(DOS2UNIX) docs/README.html
	$(RM) -f docs/README
	cd docs && asciidoc --unsafe -a toc -a toclevels=2 -a max-width=800 QUICKSTART
	chmod 644 docs/QUICKSTART.html
	$(DOS2UNIX) docs/QUICKSTART.html

rpm: dist
	mkdir -p $(RPM_TOPDIR)/{SOURCES,BUILD,RPMS,SRPMS,SPECS}
	cp mod_gearman-$(VERSION).tar.gz $(RPM_TOPDIR)/SOURCES
	rpmbuild -ba --define "_topdir $(RPM_TOPDIR)" \
	    --buildroot=$$(pwd)/rpm.buildroot support/mod_gearman.spec
	mv -v $(RPM_TOPDIR)/RPMS/*/*.rpm .
	mv -v $(RPM_TOPDIR)/SRPMS/*.src.rpm .
	$(RM) -f $(RPM_TOPDIR) rpm.buildroot

deb:
	dpkg-buildpackage -us -uc

mrproper:
	git clean -xfd

version:
	if [ -z "$$NEWVERSION" ]; then NEWVERSION=$$(dialog --stdout --inputbox "New Version:" 0 0 "$(VERSION)"); fi; \
	if [ -n "$$NEWVERSION" ] && [ "$$NEWVERSION" != "$(VERSION)" ]; then \
		sed -ri "s/$(VERSION)/$$NEWVERSION/" include/common.h configure.ac docs/QUICKSTART support/mod_gearman.spec; \
		sed -i Changes -e "s/$(VERSION)/$$NEWVERSION $(shell date)\n          - ...\n\n$(VERSION)/"; \
		$(MAKE) docs; \
	fi;

# order does matter for perl libs/includes (at least on centos)
mod_gearman_worker$(EXEEXT): $(mod_gearman_worker_OBJECTS) $(perl_objects) $(mod_gearman_worker_DEPENDENCIES)
	@rm -f mod_gearman_worker$(EXEEXT)
	$(CCLD) $(AM_CFLAGS) $(CFLAGS) -o $@ $(mod_gearman_worker_OBJECTS) $(perl_objects) $(mod_gearman_worker_LDADD) $(LIBS) $(PERLLIB)
