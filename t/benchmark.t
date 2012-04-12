#!/usr/bin/perl

use warnings;
use strict;
use Test::More tests => 8;
use Data::Dumper;
use Time::HiRes qw( gettimeofday tv_interval sleep );

my $TESTPORT    = 54730;
my $NR_TST_JOBS = 2000;

# check requirements
ok(-f './mod_gearman_worker', 'worker present') or BAIL_OUT("no worker!");
chomp(my $gearmand = `which gearmand 2>/dev/null`);
isnt($gearmand, '', 'gearmand present: '.$gearmand) or BAIL_OUT("no gearmand");

system("$gearmand --port $TESTPORT --pid-file=./gearman.pid -d");
chomp(my $gearmand_pid = `cat ./gearman.pid`);

isnt($gearmand_pid, '', 'gearmand running: '.$gearmand_pid) or BAIL_OUT("no gearmand");

# fill the queue
my $t0 = [gettimeofday];
for my $x (1..$NR_TST_JOBS) {
    `./send_gearman --server=localhost:$TESTPORT --host=test --message="test" --result_queue=eventhandler`;
}
my $elapsed = tv_interval ( $t0 );
my $rate    = int($NR_TST_JOBS / $elapsed);
ok($elapsed, 'filling gearman queue with '.$NR_TST_JOBS.' jobs took: '.$elapsed.' seconds');
ok($rate > 100, 'fill rate '.$rate.'/s');

#diag(`./gearman_top -b -H localhost:$TESTPORT`);

# now clear the queue
`>worker.log`;
$t0 = [gettimeofday];
system("./mod_gearman_worker --server=localhost:$TESTPORT --debug=0 --max-worker=1 --encryption=off --p1_file=./worker/mod_gearman_p1.pl --daemon --pidfile=./worker.pid --logfile=./worker.log");
chomp(my $worker_pid = `cat ./worker.pid`);
isnt($worker_pid, '', 'worker running: '.$worker_pid);

while(get_queue("eventhandler")->{'waiting'} != 0) {
    sleep(0.1);
}

$elapsed = tv_interval ( $t0 );
$rate    = int($NR_TST_JOBS / $elapsed);
ok($elapsed, 'cleared gearman queue in '.$elapsed.' seconds');
ok($rate > 500, 'clear rate '.$rate.'/s');

#diag(`./gearman_top -b -H localhost:$TESTPORT`);

# clean up
`kill $worker_pid`;
`kill $gearmand_pid`;

exit(0);

#################################################
sub get_queue {
    my $queue = shift;
    my $out = `./gearman_top -b -H localhost:$TESTPORT`;
    if($out =~ m/^\s*$queue\s*\|\s*(\d+)\s*\|\s*(\d+)\s*\|\s*(\d+)/mx) {
        return({
            worker  => $1,
            waiting => $2,
            running => $3,
        });
    }
    return({
        worker  => -1,
        waiting => -1,
        running => -1,
    });
}

