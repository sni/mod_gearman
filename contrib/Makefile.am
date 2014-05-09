mod_gearman2_mini_epn: contrib/mod_gearman_mini_epn.c
	perl -MExtUtils::Embed -e xsinit
	$(CC) $(CFLAGS) -c perlxsi.c  `perl -MExtUtils::Embed -e ccopts`
	$(CC) $(CFLAGS) -c contrib/mod_gearman_mini_epn.c `perl -MExtUtils::Embed -e ccopts`
	$(CC) $(CFLAGS) $(LDFLAGS) perlxsi.o mod_gearman_mini_epn.o `perl -MExtUtils::Embed -e ccopts -e ldopts` -o $@
