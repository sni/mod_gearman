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

AM_CPPFLAGS=-Iinclude $(CFLAGS)
AM_CFLAGS =-DDATADIR='"$(datadir)"'
LIBS = $(GEARMAN_LIBS) $(openssl_LIBS)
OS=$(shell uname)
# disable parallel builds which sometime run into ranlib file not found errors
MAKEFLAGS=-j1
DAILYVERSION=$(shell ./get_version)

if USEPERL
P1FILE           = worker/mod_gearman_p1.pl
PERLLIB          = @PERLLIBS@
perl_objects     = common/perlxsi.o common/epn_utils.o
install_epnfiles = install_epnfiles
EXTRA_mod_gearman_worker_SOURCES=common/perlxsi.c common/epn_utils.c
EXTRA_01_utils_SOURCES=common/perlxsi.c common/epn_utils.c
EXTRA_02_full_SOURCES=common/perlxsi.c common/epn_utils.c
EXTRA_03_exec_SOURCES=common/perlxsi.c common/epn_utils.c
EXTRA_06_exec_SOURCES=common/perlxsi.c common/epn_utils.c
EXTRA_07_epn_SOURCES=common/perlxsi.c common/epn_utils.c
EPN_BIN = mod_gearman_mini_epn
else
PERLLIB          =
P1FILE           =
install_epnfiles =
perl_objects     =
EPN_BIN          =
endif

# source definitions
common_SOURCES             = common/gm_crypt.c  \
                             common/gearman_utils.c \
                             common/utils.c \
                             common/gm_alloc.c

common_check_SOURCES       = common/check_utils.c \
                             common/popenRWE.c \
                             worker/worker_client.c

pkglib_LIBRARIES           =
NEB_MODULES                =
pkglib_LIBRARIES          += mod_gearman_naemon.so
mod_gearman_naemon_so_SOURCES = $(common_SOURCES) \
                             neb_module_naemon/result_thread.c \
                             neb_module_naemon/mod_gearman.c
NEB_MODULES               += mod_gearman_naemon.o

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
gearman_top_LDADD          = $(LDFLAGS) -lncurses

# tests
check_PROGRAMS   = 01_utils 02_full 03_exec 04_log 05_neb 06_exec 07_epn
#check_PROGRAMS  += 08_roundtrip
01_utils_SOURCES = $(common_SOURCES) t/tap.h t/tap.c t/01-utils.c $(common_check_SOURCES)
02_full_SOURCES  = $(common_SOURCES) t/tap.h t/tap.c t/02-full.c $(common_check_SOURCES)
03_exec_SOURCES  = $(common_SOURCES) t/tap.h t/tap.c t/03-exec_checks.c $(common_check_SOURCES)
04_log_SOURCES   = $(common_SOURCES) t/tap.h t/tap.c t/04-log.c
05_neb_SOURCES   = $(common_SOURCES) t/tap.h t/tap.c t/05-neb.c
05_neb_LDFLAGS   = -Wl,--export-dynamic -rdynamic
05_neb_LDADD     = $(NAEMON_LDFLAGS) -ldl
07_epn_SOURCES   = $(common_SOURCES) t/tap.h t/tap.c t/07-epn.c $(common_check_SOURCES)
# only used for performance tests
06_exec_SOURCES  = $(common_SOURCES) t/tap.h t/tap.c t/06-execvp_vs_popen.c $(common_check_SOURCES)
#08_roundtrip_SOURCES  = $(common_SOURCES) t/08-roundtrip.c
#08_roundtrip_LDFLAGS = -Wl,--export-dynamic -rdynamic
TESTS            = $(check_PROGRAMS) t/09-benchmark.t t/10-large-result.t t/11-alloc.t t/12-cppcheck.t t/13-tools.t t/14-symbols.t


GEARMANDS=/usr/sbin/gearmand /opt/sbin/gearmand
replace_vars = sed -e 's:%CONFIG_WORKER%:$(sysconfdir)/mod_gearman/worker.conf:g' \
               -e 's:%PIDFILE%:$(localstatedir)/mod_gearman/mod_gearman_worker.pid:g' \
               -e 's:%LOGFILE_NEB%:$(localstatedir)/log/mod_gearman/mod_gearman_neb.log:g' \
               -e 's:%LOGFILE_WORKER%:$(localstatedir)/log/mod_gearman/mod_gearman_worker.log:g' \
               -e 's:%P1FILE%:$(datadir)/mod_gearman/mod_gearman_p1.pl:g' \
               -e 's:%WORKERBIN%:$(bindir)/mod_gearman_worker:g' \
               -e 's:%USER%:$(user):g'

EXTRA_DIST = COPYING etc/*.in extras include \
             THANKS README docs/README.html Changes worker/initscript.in worker/daemon-systemd.in \
             support/mod_gearman.spec \
             t/data/* t/rc t/both t/killer t/sleep t/*.pl t/*.t \
             worker/mod_gearman_p1.pl t/test_all.pl t/valgrind_suppress.cfg contrib \
             etc/mod_gearman_logrotate ./autogen.sh \
             debian

export ARFLAGS="cr" # avoid: ar: `u' modifier ignored since `D' is the default (see `U')

include contrib/Makefile.am

# other targets
mod_gearman_naemon.o: $(mod_gearman_naemon_so_OBJECTS) $(mod_gearman_naemon_so_DEPENDENCIES)
	@echo '    $$(CC) $<'
	@if [ "$(OS)" = "Darwin" ]; then \
		$(CXX) $(LDFLAGS) -dynamiclib -single_module -undefined dynamic_lookup $(mod_gearman_naemon_so_OBJECTS) -o $@ -lpthread -lgearman $(openssl_LIBS); \
	else \
		$(CXX) $(LDFLAGS) -fPIC -shared $(mod_gearman_naemon_so_OBJECTS) -o $@ -lpthread -lgearman $(openssl_LIBS); \
	fi
	chmod 644 mod_gearman_naemon.o
	@$(RM) mod_gearman_naemon.so

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
	@$(replace_vars) worker/daemon-systemd.in > worker/daemon-systemd

all-local: $(NEB_MODULES) initscript-local mod_gearman_neb.conf-local mod_gearman_worker.conf-local $(EPN_BIN)
	@echo ""
	@echo "################################################################"
	@echo ""
	@echo "  the following files have been created:"
	@echo ""
	@echo "  mod_gearman_naemon.o"
	@echo "  mod_gearman_worker"
	@echo "  worker/initscript"
	@echo "  worker/daemon-systemd"
	@echo "  etc/mod_gearman_neb.conf"
	@echo "  etc/mod_gearman_worker.conf"
	@[ "$(P1FILE)"  = "" ] || echo "  $(P1FILE)"
	@[ "$(EPN_BIN)" = "" ] || echo "  $(EPN_BIN)"
	@echo ""
	@echo "  read the README for configuration details"
	@echo ""
	@echo "  for a normal installation continue with"
	@echo "  make install"
	@echo ""
	@echo "################################################################"

distclean-local: clean
	$(RM) -rf .deps/ Makefile.in aclocal.m4 autom4te.cache config.* configure depcomp install-sh missing *.gz contrib/.deps/ contrib/.dirstamp

clean-local:
	$(RM) -f worker.static \
	         worker/initscript \
	         worker/daemon-systemd \
	         etc/mod_gearman.conf \
	         rpm.topdir \
	         *.o \
	         */*.o \
	         etc/mod_gearman_neb.conf \
	         etc/mod_gearman_worker.conf \
	         mod_gearman_mini_epn perlxsi.c

worker.static: worker
	@echo "################################################################"
	@echo ""
	@echo " if the static compiler complains about 'cannot find -lgearman', you have to"
	@echo " compile libgearman with -static"
	@echo ""
	@echo "################################################################"
	cd worker && $(CC) $(LDFLAGS) $(CFLAGS) -static -o worker.static $(worker_OBJECTS) -lpthread -luuid
	@echo ""
	@echo " worker.static created."
	@echo ""

install-exec-local: install-local-state-dir
	$(install_sh_PROGRAM) -m 644 mod_gearman_naemon.o $(DESTDIR)$(pkglibdir)/mod_gearman_naemon.o
	$(install_sh_PROGRAM) -m 755 worker/initscript $(DESTDIR)$(initrddir)/mod-gearman-worker

install-local-state-dir:
	$(install_sh_PROGRAM) -d $(DESTDIR)$(localstatedir)/mod_gearman/

install_epnfiles:
	$(install_sh_PROGRAM) -m 755 $(P1FILE)  $(DESTDIR)$(datadir)/mod_gearman/mod_gearman_p1.pl
	$(install_sh_PROGRAM) -m 755 $(EPN_BIN) $(DESTDIR)$(bindir)/mod_gearman_mini_epn
	@echo "################################################################"
	@echo " Installation completed:"
	@echo " p1file:         $(DESTDIR)$(datadir)/mod_gearman/mod_gearman_p1.pl"
	@echo "################################################################"

install-data-local: install-local-state-dir $(install_epnfiles)
	$(install_sh_PROGRAM) -d $(DESTDIR)$(localstatedir)/log/mod_gearman
	@echo ""
	@echo "################################################################"
	@echo ""
	@echo " Installation completed:"
	@echo " neb module:     $(DESTDIR)$(pkglibdir)/mod_gearman_naemon.o"
	@echo ""
	@echo " worker:         $(DESTDIR)$(bindir)/mod_gearman_worker"
	@echo " init script:    $(DESTDIR)$(initrddir)/mod-gearman-worker"
	@[ "$(EPN_BIN)" = "" ] || echo " mini-epn:       $(DESTDIR)$(bindir)/mod_gearman_mini_epn"
	@echo ""
	@echo " check bin:      $(DESTDIR)$(bindir)/check_gearman"
	@echo " send bin:       $(DESTDIR)$(bindir)/send_gearman"
	@echo " send multi bin: $(DESTDIR)$(bindir)/send_multi"
	@echo ""
	@echo "just add the broker line to your naemon.cfg:"
	@echo "broker_module=$(DESTDIR)$(pkglibdir)/mod_gearman_naemon.o config=$(DESTDIR)$(sysconfdir)/mod_gearman/module.conf"
	@echo ""
	@if [ -e $(DESTDIR)$(sysconfdir)/mod_gearman/module.conf -o -e $(DESTDIR)$(sysconfdir)/mod_gearman/worker.conf ]; then \
	    echo " install default config with"; \
	    echo ""; \
	    echo " make install-config"; \
	    echo ""; \
	else \
            make install-config; \
	fi
	$(RM) $(DESTDIR)$(pkglibdir)/mod_gearman_naemon.so


install-config:
	$(install_sh_PROGRAM) -m 644 etc/mod_gearman_neb.conf    $(DESTDIR)$(sysconfdir)/mod_gearman/module.conf
	$(install_sh_PROGRAM) -m 644 etc/mod_gearman_worker.conf $(DESTDIR)$(sysconfdir)/mod_gearman/worker.conf
	$(install_sh_PROGRAM) -m 644 etc/mod_gearman_logrotate   $(DESTDIR)$(sysconfdir)/logrotate.d/mod_gearman
	@echo "################################################################"
	@echo ""
	@echo " configuration:"
	@echo "         neb module:  $(DESTDIR)$(sysconfdir)/mod_gearman/module.conf"
	@echo "         worker:      $(DESTDIR)$(sysconfdir)/mod_gearman/worker.conf"
	@echo "         logrotate.d: $(DESTDIR)$(sysconfdir)/logrotate.d/mod_gearman"
	@echo ""
	@echo "################################################################"

test: check
	@echo "################################################################"
	@echo ""
	@echo " All tests completed successfully"
	@echo ""
	@echo "################################################################"
	@rm -f *.trs *.log t/*.log t/*.trs

fulltest:
	./t/test_all.pl
	@echo "################################################################"
	@echo ""
	@echo " Full tests completed successfully"
	@echo ""
	@echo "################################################################"
	@rm -f *.trs *.log t/*.log t/*.trs

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
	./replace_doc_toc.pl docs/README.html
	$(RM) -f docs/README

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

versionprecheck:
	[ -e .git ] || { echo "changing versions only works in git clones!"; exit 1; }
	[ `git status | grep -cP 'working (directory|tree) clean'` -eq 1 ] || { echo "git project is not clean, cannot tag version"; exit 1; }

newversion: versionprecheck
	test -e .git
	make NEWVERSION="$(DAILYVERSION)" version

dailyversion: newversion
	git checkout Changes

resetdaily:
	git reset --hard HEAD
	git checkout .

version:
	if [ -z "$$NEWVERSION" ]; then NEWVERSION=$$(dialog --stdout --inputbox "New Version:" 0 0 "$(VERSION)"); fi; \
	if [ -n "$$NEWVERSION" ] && [ "$$NEWVERSION" != "$(VERSION)" ]; then \
		sed -ri "s/$(VERSION)/$$NEWVERSION/" include/common.h configure.ac support/mod_gearman.spec; \
		sed -i Changes -e "s/next:/$$NEWVERSION $(shell LC_TIME=C date)/"; \
		$(MAKE) docs; \
		if [ "x$$DEBEMAIL" = "x" ]; then \
			DEBEMAIL="Sven Nierlein <sven@consol.de>"; \
		fi; \
		if [ "x$$DEBFULLNAME" = "x" ]; then \
			DEBFULLNAME="Sven Nierlein"; \
		fi; \
		sed -e 's/UNRELEASED/unstable/g' -i debian/changelog; \
		dch --newversion "$$NEWVERSION" --package "mod-gearman" -D "UNRELEASED" --urgency "low" "new upstream release"; \
		sed -e 's/unstable/UNRELEASED/g' -i debian/changelog; \
	fi;

dailydist: cleandist
	$(MAKE) newversion
	$(MAKE) dist
	$(MAKE) resetdaily
	ls -la *.gz

cleandist:
	rm -f *.gz

releasedist: cleandist dist
	git describe --tag --exact-match
	ls -la *.gz

# order does matter for perl libs/includes (at least on centos 5)
mod_gearman_worker$(EXEEXT): $(mod_gearman_worker_OBJECTS) $(perl_objects) $(mod_gearman_worker_DEPENDENCIES)
	@rm -f mod_gearman_worker$(EXEEXT)
	$(AM_V_CCLD)$(LINK) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(mod_gearman_worker_OBJECTS) $(perl_objects) $(mod_gearman_worker_LDADD) $(LIBS) $(PERLLIB)

01_utils$(EXEEXT): $(01_utils_OBJECTS) $(perl_objects) $(01_utils_DEPENDENCIES)
	@rm -f 01_utils$(EXEEXT)
	$(AM_V_CCLD)$(LINK) $(AM_CFLAGS) $(CFLAGS) -o $@ $(01_utils_OBJECTS) $(perl_objects) $(01_utils_LDADD) $(LIBS) $(PERLLIB)

02_full$(EXEEXT): $(02_full_OBJECTS) $(perl_objects) $(02_full_DEPENDENCIES)
	@rm -f 02_full$(EXEEXT)
	$(AM_V_CCLD)$(LINK) $(AM_CFLAGS) $(CFLAGS) -o $@ $(02_full_OBJECTS) $(perl_objects) $(02_full_LDADD) $(LIBS) $(PERLLIB)

03_exec$(EXEEXT): $(03_exec_OBJECTS) $(perl_objects) $(03_exec_DEPENDENCIES)
	@rm -f 03_exec$(EXEEXT)
	$(AM_V_CCLD)$(LINK) $(AM_CFLAGS) $(CFLAGS) -o $@ $(03_exec_OBJECTS) $(perl_objects) $(03_exec_LDADD) $(LIBS) $(PERLLIB)

06_exec$(EXEEXT): $(06_exec_OBJECTS) $(perl_objects) $(06_exec_DEPENDENCIES)
	@rm -f 06_exec$(EXEEXT)
	$(AM_V_CCLD)$(LINK) $(AM_CFLAGS) $(CFLAGS) -o $@ $(06_exec_OBJECTS) $(perl_objects) $(06_exec_LDADD) $(LIBS) $(PERLLIB)

07_epn$(EXEEXT): $(07_epn_OBJECTS) $(perl_objects) $(07_epn_DEPENDENCIES)
	@rm -f 07_epn$(EXEEXT)
	$(AM_V_CCLD)$(LINK) $(AM_CFLAGS) $(CFLAGS) -o $@ $(07_epn_OBJECTS) $(perl_objects) $(07_epn_LDADD) $(LIBS) $(PERLLIB)
