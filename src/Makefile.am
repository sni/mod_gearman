
###############################################################################
#
# mod_gearman - distribute checks with gearman
#
# Copyright (c) 2010 Sven Nierlein
#
###############################################################################

AM_CPPFLAGS=-Iincludes

pkglib_LIBRARIES = mod_gearman.so
mod_gearman_so_SOURCES = gearman.c utils.c logger.c result_thread.c mod_gearman.c

bin_PROGRAMS = worker
worker_SOURCES = gearman.c worker_logger.c worker_client.c utils.c worker.c

# -g adds debug symbols, -s strips
CFLAGS=-Wall -W -Wstrict-prototypes -g
#mod_gearman_so_CXXFLAGS = $(CFLAGS) -fPIC -O
mod_gearman_so_CXXFLAGS = $(CFLAGS) -fPIC -O
mod_gearman_so_CFLAGS   = $(mod_gearman_so_CXXFLAGS)


OS=`uname`

mod_gearman.o: $(mod_gearman_so_OBJECTS) $(mod_gearman_so_DEPENDENCIES)
	if [ "$(OS)" == "Darwin" ]; then \
	    $(CXX) $(LDFLAGS) -dynamiclib -single_module -undefined dynamic_lookup $(mod_gearman_so_OBJECTS) -o $@ -lpthread -lgearman; \
	else \
	    $(CXX) $(LDFLAGS) -fPIC -shared $(mod_gearman_so_OBJECTS) -o $@ -lpthread -lgearman -s; \
	fi
	rm -f mod_gearman.so

all-local: mod_gearman.o
	@echo ""
	@echo "################################################################"
	@echo ""
	@echo "  now copy the src/mod_gearman.o to your nagios folder"
	@echo "  and at the broker= line to your config."
	@echo ""
	@echo "  read the README for configuration details"
	@echo ""
	@echo " for static worker continue with"
	@echo " make static-worker"
	@echo ""
	@echo "################################################################"

distclean-local: clean
	for file in .deps/ Makefile.in aclocal.m4 autom4te.cache config.* configure depcomp install-sh missing; do test -e $$file && rm -fr $$file; done

clean-local:
	rm -f worker.static

static-worker: worker
	gcc -Wall -W -Wstrict-prototypes -g -static -o worker.static $(worker_OBJECTS) -lgearman -lpthread -luuid
	@echo ""
	@echo " worker.static created."
	@echo ""
