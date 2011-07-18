#!/usr/bin/perl

use warnings;
use strict;
use Test::More;

`type valgrind >/dev/null 2>&1`;
if($? != 0) {
    plan skip_all => 'valgrind required';
}
plan(tests => 13);

`make`;
is($?, 0, "build rc is $?");

my $vallog  = '/tmp/valgrind.log';
my $testlog = '/tmp/mod_gearman_test.log';
for my $test (qw(01_utils 02_full 03_exec 04_log)) {
    `make $test`;
    is($?, 0, "$test build rc is $?");

    `valgrind --tool=memcheck --leak-check=yes --leak-check=full --show-reachable=yes --track-origins=yes --log-file=/tmp/valgrind.log ./$test >$testlog 2>&1`;
    is($?, 0, "$test valgrind exit code is $?") or diag(`cat $testlog`);

    is(qx(grep "ERROR SUMMARY: " $vallog | grep -v "ERROR SUMMARY: 0 errors"), "", "valgrind Error Summary")
      or BAIL_OUT("check memory $test in $vallog");
}

unlink($vallog);
unlink($testlog);
