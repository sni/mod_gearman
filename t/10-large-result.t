#!/usr/bin/perl

use warnings;
use strict;
use File::Temp qw/tempfile/;
use Test::More tests => 9;

alarm(60); # hole test should not take longer than 60 seconds
$SIG{'ALRM'} = sub { cleanup(); die("ALARM"); };

my $TESTPORT    = 54730;
my $LOGFILE     = "/tmp/gearmand_large_result.log";
my $PAYLOADSIZE = 1000000;

################################################################################
# PREPARATION
# check requirements
ok(-f './mod_gearman_worker', 'worker present') or BAIL_OUT("no worker!");
chomp(my $gearmand = `which gearmand 2>/dev/null`);
ok($gearmand, 'gearmand present: '.$gearmand) or BAIL_OUT("no gearmand");

chomp(my $gearman = `which gearman 2>/dev/null`);
ok($gearman, 'gearman present: '.$gearman) or BAIL_OUT("no gearman");

# start gearmand
system("$gearmand --port $TESTPORT --pid-file=./gearman.pid -d --log-file=$LOGFILE");
chomp(my $gearmand_pid = `cat ./gearman.pid`);
isnt($gearmand_pid, '', 'gearmand running: '.$gearmand_pid) or BAIL_OUT("no gearmand");

# start worker
my $cmd = "./mod_gearman_worker --server=localhost:$TESTPORT --debug=4 --max-worker=1 --encryption=off --p1_file=./worker/mod_gearman_p1.pl --daemon --pidfile=./worker.pid --logfile=$LOGFILE";
system($cmd);
chomp(my $worker_pid = `cat ./worker.pid 2>/dev/null`);
isnt($worker_pid, '', 'worker running: '.$worker_pid);

# prepare large result
my($fh, $resultfile) = tempfile();
print $fh "type=passive\n";
print $fh "host_name=test\n";
print $fh "start_time=".time().".000\n";
print $fh "finish_time=".time().".000\n";
print $fh "return_code=2\n";
print $fh "service_description=test\n";
print $fh "output=";
print $fh "x"x$PAYLOADSIZE;
print $fh "yz\n";
close($fh);

################################################################################
# TEST
ok(-s $resultfile > 1000, "resultfile has size: ".int((-s $resultfile)/1024)."KB");
sleep(1); # give everything some time to start
my $top1 = `./gearman_top -H localhost:$TESTPORT -b`;
like($top1, '/host \s*\|\s*1\s*|\s*0\s*|\s*0/', "worker present");
`cat $resultfile | base64 | gearman -f host -h localhost -p $TESTPORT -b`;
sleep(2);
my $top2 = `./gearman_top -H localhost:$TESTPORT -b`;
like($top2, '/host \s*\|\s*1\s*|\s*0\s*|\s*0/', "worker alive and empty queue");
my $killed = kill(0, $worker_pid);
is($killed, 1, "worker still alive");

################################################################################
# CLEAN UP
cleanup();
exit(0);

################################################################################
sub cleanup {
    `kill $worker_pid`;
    `kill $gearmand_pid`;
    unlink($LOGFILE);
    unlink($resultfile);
}
