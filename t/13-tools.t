#!/usr/bin/perl

use warnings;
use strict;
use Test::More tests => 4;
use Data::Dumper;
use POSIX;
use IPC::Open3 qw/open3/;

my($rc, $out) = _run_command("./send_gearman --server=127.0.0.99:65424 --host=test --message=test -r=0");
is($rc, 3, "return code unknown");
like($out, "/\Qsending job to gearmand failed:\E/", "output ok");

($rc, $out) = _run_command("./send_gearman --server=127.0.0.99:65424 --host=test --message=test -r=0 --encryption=yes --key=12345678");
is($rc, 3, "return code unknown");
like($out, "/\Qsending job to gearmand failed:\E/", "output ok");

################################################################################
sub _run_command {
    my($cmd, $stdin) = @_;

    my($rc, $output);
    if(ref $cmd eq 'ARRAY') {
        my $prog = shift @{$cmd};
        my($pid, $wtr, $rdr, @lines);
        $pid = open3($wtr, $rdr, $rdr, $prog, @{$cmd});
        if($stdin) {
            print $wtr $stdin,"\n";
            CORE::close($wtr);
        }
        while(POSIX::waitpid($pid, WNOHANG) == 0) {
            push @lines, <$rdr>;
        }
        $rc = $?;
        push @lines, <$rdr>;
        chomp($output = join('', @lines) || '');
    } else {
        confess("stdin not supported for string commands") if $stdin;
        $output = `$cmd 2>&1`;
        $rc = $?;
    }
    if($rc == -1) {
        $output .= "[".$!."]";
    } else {
        $rc = $rc>>8;
    }
    return($rc, $output);
}