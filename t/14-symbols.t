#!/usr/bin/perl

use warnings;
use strict;
use Test::More tests => 3;

my $mods = {
    "mod_gearman_naemon.o"  => 0,
    "mod_gearman_nagios3.o" => 1,
    "mod_gearman_nagios4.o" => 0,
};
for my $mod (sort keys %{$mods}) {
SKIP: {
    skip "$mod does not exist", 1 if !-s $mod;
    chomp(my $count = `grep -bc check_result_info $mod`);
    if($mods->{$mod}) {
        ok($count > 0, "found $count matches of check_result_info in $mod");
    } else {
        ok($count == 0, "found $count matches of check_result_info in $mod");
    }
};
}
