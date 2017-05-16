#!/usr/bin/perl

use warnings;
use strict;
use Test::More tests => 6;

my $mods = {
    "mod_gearman_naemon.o"  => { 'check_result_info' => 0, 'notification_reason_name' => 1 },
    "mod_gearman_nagios3.o" => { 'check_result_info' => 1, 'notification_reason_name' => 0 },
    "mod_gearman_nagios4.o" => { 'check_result_info' => 0, 'notification_reason_name' => 1 },
};
for my $mod (sort keys %{$mods}) {
SKIP: {
    skip "$mod does not exist", 1 if !-s $mod;
    for my $sym (sort keys %{$mods->{$mod}}) {
        chomp(my $count = `grep -bc $sym $mod`);
        if($mods->{$mod}->{$sym}) {
            ok($count > 0, "found $count matches of $sym in $mod");
        } else {
            ok($count == 0, "found $count matches of $sym in $mod");
        }
    }
};
}
