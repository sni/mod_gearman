
###############################################################################
#
# mod_gearman - distribute checks with gearman
#
# Copyright (c) 2010 Sven Nierlein
#
###############################################################################

bin_PROGRAMS = worker
worker_SOURCES = worker_logger.c worker_client.c worker.c

worker_LDADD = ../common/libcmn.la

distclean:

static-worker: worker
	$(CC) $(LDFLAGS) $(CFLAGS) -static -o worker.static $(worker_OBJECTS) -lgearman -lpthread -luuid
	@echo ""
	@echo " worker.static created."
	@echo ""
