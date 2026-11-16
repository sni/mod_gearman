#!/usr/bin/perl

use warnings;
use strict;
use Test::More;

my $cppcheck;
for my $check (qw|/usr/bin/cppcheck
                  /usr/local/bin/cppcheck
                 |) {
    if(-x $check) {
        $cppcheck = $check;
        last;
    }
}

plan skip_all => 'requires cppcheck' unless $cppcheck;
plan tests    => 2;

# naemon defines NEB_API_VERSION as a function-like macro (src/naemon/nebmodules.h),
# so cppcheck must see it in the same form, not as an object-like macro
my $cmd = $cppcheck." --cppcheck-build-dir=/tmp --force --inline-suppr --template='{file}:{line},{severity},{id},{message}' '-D NEB_API_VERSION(x)=int __neb_api_version=x' common/. include/. neb_module_*/. tools/. worker/. 2>&1";
my $out = `$cmd`;
ok($? == 0, $cmd);
$out = join "\n", grep {
    /\S/
        && !/^Checking .+\.\.\.$/
        && !/^\d+\/\d+ files checked \d+% done$/
} split /\n/, $out;
if($out eq "") {
    ok(1, "cppcheck: no errors detected");
} else {
    fail($out);
}
