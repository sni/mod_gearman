
###############################################################################
#
# mod_gearman - distribute checks with gearman
#
# Copyright (c) 2010 Sven Nierlein
#
###############################################################################


pkglib_LIBRARIES = mod_gearman.so
mod_gearman_so_SOURCES = utils.c logger.c worker.c mod_gearman.c

mod_gearman_so_CXXFLAGS = $(CFLAGS) -Iincludes -fPIC
mod_gearman_so_CFLAGS   = $(mod_gearman_so_CXXFLAGS)
LDFLAGS=-L/tmp/gearmand-0.14/libgearman/.libs

mod_gearman.o: $(mod_gearman_so_OBJECTS) $(mod_gearman_so_DEPENDENCIES)
	$(CXX) $(LDFLAGS) -s -fPIC -shared $(mod_gearman_so_OBJECTS) -o $@ -lpthread -lgearman
	rm -f mod_gearman.so

all-local: mod_gearman.o

distclean-local: clean
	for file in .deps/ Makefile Makefile.in aclocal.m4 autom4te.cache config.* configure depcomp install-sh missing stamp-h1; do test -e $$file && rm -r $$file; done
