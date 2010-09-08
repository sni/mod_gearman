
SUBDIRS = common neb_module worker

distclean-local: clean
	for file in .deps/ Makefile.in aclocal.m4 autom4te.cache config.* configure depcomp install-sh missing; do rm -fr $$file; done

#for file in .deps/ Makefile.in aclocal.m4 autom4te.cache config.* configure depcomp install-sh missing; do test -e $$file && rm -fr $$file; done
