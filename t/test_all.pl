#!/usr/bin/perl

use warnings;
use strict;
use Test::More;

# clean up old gearmands
`ps -efl | grep gearmand | grep 54730 | awk '{ print \$4 }' | xargs kill`;

`type valgrind >/dev/null 2>&1`;
if($? != 0) {
    plan skip_all => 'valgrind required';
}

my $cmd = 'make clean  2>&1 && make 2>&1';
ok(1, $cmd);
my $makeout = `$cmd`;
is($?, 0, "build rc is $?") or BAIL_OUT("no need to test without successful make!\n".$makeout);

my $skip_perl_mem_leaks = "";
if(`grep -c '^#define EMBEDDEDPERL' config.h` > 0) {
    $skip_perl_mem_leaks = "--suppressions=./t/valgrind_suppress.cfg";
}

my $vallog       = '/tmp/valgrind.log';
my $testlog      = '/tmp/mod_gearman_test.log';
my $suppressions = '/tmp/suppressions.log';
`>$suppressions`;
my @tests = $ARGV[0] || split/\s+/, `grep ^check_PROGRAMS Makefile.am | awk -F = '{print \$2}'`;
for my $test (@tests) {
    next if $test =~ m/^\s*$/;
    my $cmd = "make $test 2>/dev/null";
    ok(1, $cmd);
    `$cmd`;
    is($?, 0, "$test build rc is $?");

    $cmd = "yes | valgrind --tool=memcheck --leak-check=yes --leak-check=full --show-reachable=yes --track-origins=yes $skip_perl_mem_leaks --gen-suppressions=yes --error-limit=no --log-file=$vallog ./$test >$testlog 2>&1";
    ok(1, $cmd);
    `$cmd`;
    is($?, 0, "$test valgrind exit code is $?") or diag(`cat $testlog`);

    `cat $vallog >> $suppressions`;

    is(qx(grep "ERROR SUMMARY: " $vallog | grep -v "ERROR SUMMARY: 0 errors"), "", "valgrind Error Summary")
      or BAIL_OUT("valgrind output for test $test from\n$vallog:\n".`cat $vallog`."failed test: $test\n");
}

unlink($vallog);
unlink($testlog);

#diag(`ls -la $suppressions`);

done_testing();
