mod_gearman_mini_epn: contrib/mod_gearman_mini_epn.c
	perl -MExtUtils::Embed -e xsinit
	$(AM_V_CC)$(CC) $(AM_CFLAGS) $(CFLAGS) -c perlxsi.c  `perl -MExtUtils::Embed -e ccopts`
	$(AM_V_CC)$(CC) $(AM_CFLAGS) $(CFLAGS) -c contrib/mod_gearman_mini_epn.c `perl -MExtUtils::Embed -e ccopts`
	$(AM_V_CC)$(CC) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) perlxsi.o mod_gearman_mini_epn.o `perl -MExtUtils::Embed -e ccopts -e ldopts` -o $@
