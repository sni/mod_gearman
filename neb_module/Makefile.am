
###############################################################################
#
# mod_gearman - distribute checks with gearman
#
# Copyright (c) 2010 Sven Nierlein
#
###############################################################################

.PHONY: clean distclean

pkglib_LIBRARIES = mod_gearman.so
mod_gearman_so_SOURCES = utils.c logger.c worker.c mod_gearman.c

# -g adds debug symbols
#mod_gearman_so_CXXFLAGS = $(CFLAGS) -Iincludes -fPIC -Wall -W -Wstrict-prototypes -O
mod_gearman_so_CXXFLAGS = $(CFLAGS) -Iincludes -fPIC -Wall -W -Wstrict-prototypes -O -g
mod_gearman_so_CFLAGS   = $(mod_gearman_so_CXXFLAGS)
LDFLAGS=-L/tmp/gearmand-0.14/libgearman/.libs

mod_gearman.o: $(mod_gearman_so_OBJECTS) $(mod_gearman_so_DEPENDENCIES)
	$(CXX) $(LDFLAGS)  -fPIC -shared $(mod_gearman_so_OBJECTS) -o $@ -lpthread -lgearman
# -s strips the result file
#	$(CXX) $(LDFLAGS) -s -fPIC -shared $(mod_gearman_so_OBJECTS) -o $@ -lpthread -lgearman
	rm -f mod_gearman.so

all-local: mod_gearman.o
	@echo ""
	@echo "################################################################"
	@echo ""
	@echo "  now copy the neb_module/mod_gearman.o to your nagios folder"
	@echo "  and at the broker= line to your config."
	@echo ""
	@echo "  read the README for configuration details"
	@echo ""
	@echo "################################################################"

distclean-local: clean
	for file in .deps/ Makefile.in aclocal.m4 autom4te.cache config.* configure depcomp install-sh missing; do test -e $$file && rm -r $$file; done
