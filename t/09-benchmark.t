#!/usr/bin/perl

use warnings;
use strict;
use Test::More tests => 9;
use Carp qw/confess/;
use Time::HiRes qw( gettimeofday tv_interval sleep );

$SIG{'ALRM'} = sub { confess("alarm clock"); };
$SIG{'PIPE'} = sub { confess("got sig pipe"); };
alarm(60); # hole test should not take longer than 1 minute

my $TESTPORT    = 54730;
my $NR_TST_JOBS = 2000;

# check requirements
ok(-f './mod_gearman_worker', 'worker present') or BAIL_OUT("no worker!");
chomp(my $gearmand = `which gearmand 2>/dev/null`);
isnt($gearmand, '', 'gearmand present: '.$gearmand) or BAIL_OUT("no gearmand");

system("$gearmand --port $TESTPORT --pid-file=./gearman.pid -d --log-file=/tmp/gearmand_bench.log");
chomp(my $gearmand_pid = `cat ./gearman.pid`);

isnt($gearmand_pid, '', 'gearmand running: '.$gearmand_pid) or BAIL_OUT("no gearmand");

# double check gearmand
my($gearman_running, $gearman_top_out);
for(1..10) {
    $gearman_top_out = `gearman_top -b -H localhost:$TESTPORT 2>&1`;
    if($gearman_top_out =~ m/\s+\-\s+localhost:$TESTPORT\s+\-\s+v.*Waiting/sgmx) {
        $gearman_running = 1;
        last;
    }
    sleep(1);
}
ok($gearman_running, "gearmand is running");
diag($gearman_top_out) unless $gearman_running;

# fill the queue
open(my $ph, "|./send_gearman --server=localhost:$TESTPORT --result_queue=eventhandler") or die("failed to open send_gearman: $!");
my $t0 = [gettimeofday];
for my $x (1..$NR_TST_JOBS) {
    print $ph "hostname\t1\ttest\n";
}
close($ph);
my $elapsed = tv_interval ( $t0 );
my $rate    = int($NR_TST_JOBS / $elapsed);
ok($elapsed, 'filling gearman queue with '.$NR_TST_JOBS.' jobs took: '.$elapsed.' seconds');
ok($rate > 500, 'fill rate '.$rate.'/s');

# now clear the queue
`>worker.log`;
$t0 = [gettimeofday];
my $cmd = "./mod_gearman_worker --server=localhost:$TESTPORT --debug=0 --max-worker=1 --encryption=off --p1_file=./worker/mod_gearman_p1.pl --daemon --pidfile=./worker.pid --logfile=./worker.log";
system($cmd);
chomp(my $worker_pid = `cat ./worker.pid 2>/dev/null`);
isnt($worker_pid, '', 'worker running: '.$worker_pid);

wait_for_empty_queue("eventhandler");

$elapsed = tv_interval ( $t0 );
$rate    = int($NR_TST_JOBS / $elapsed);
ok($elapsed, 'cleared gearman queue in '.$elapsed.' seconds');
ok($rate > 300, 'clear rate '.$rate.'/s');

# clean up
`kill $worker_pid`;
`kill $gearmand_pid`;
unlink("/tmp/gearmand_bench.log");

exit(0);

#################################################
sub wait_for_empty_queue {
    my $queue = shift;
    open(my $ph, "./gearman_top -b -i 0.1 -H localhost:$TESTPORT |") or die("cannot launch gearman_top: $!");
    while(my $line = <$ph>) {
        if($line =~ m/^\s*$queue\s*\|\s*(\d+)\s*\|\s*(\d+)\s*\|\s*(\d+)/mx) {
            my $worker  = $1;
            my $waiting = $2;
            my $running = $3;
            if($running == 0 and $waiting == 0) {
                close($ph);
                return;
            }
        };
    }
}
