
###############################################################################
#
# mod_gearman - distribute checks with gearman
#
# Copyright (c) 2010 Sven Nierlein
#
###############################################################################

SUBDIRS = src
MAKE    = make

all: mod_gearman
	@for i in $(SUBDIRS); do \
	echo "makeing all in $$i..."; \
	(cd $$i; $(MAKE) all); done

mod_gearman:
	cd src && aclocal && autoheader && automake -a && autoconf
	cd src && ./configure

install: all
	@for i in $(SUBDIRS); do \
	echo "installing in $$i..."; \
	(cd $$i; $(MAKE) install); done

clean:
	@for i in $(SUBDIRS); do \
	echo "cleaning in $$i..."; \
	(cd $$i; $(MAKE) clean); done

distclean:
	@for i in $(SUBDIRS); do \
	echo "dist cleaning in $$i..."; \
	(cd $$i; $(MAKE) distclean); done
