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
plan(tests => 16);

my $makeout = `make clean  2>&1 && make 2>&1`;
is($?, 0, "build rc is $?") or BAIL_OUT("no need to test without successful make!\n".$makeout);

my $vallog  = '/tmp/valgrind.log';
my $testlog = '/tmp/mod_gearman_test.log';
for my $test (qw(01_utils 02_full 03_exec 04_log 05_neb 07_epn)) {
    `make $test 2>/dev/null`;
    is($?, 0, "$test build rc is $?");

    `valgrind --tool=memcheck --leak-check=yes --leak-check=full --show-reachable=yes --track-origins=yes --log-file=$vallog ./$test >$testlog 2>&1`;
    is($?, 0, "$test valgrind exit code is $?") or diag(`cat $testlog`);

    is(qx(grep "ERROR SUMMARY: " $vallog | grep -v "ERROR SUMMARY: 0 errors"), "", "valgrind Error Summary")
      or BAIL_OUT("check memory $test in $vallog");
}

unlink($vallog);
unlink($testlog);
