#!/usr/bin/perl

use warnings;
use strict;
use Gearman::Worker;
use Gearman::Client;
use threads;

#################################################
my $config = {
    'local:4730/service'        => 'remote:4730/service',
    'local:4730/host'           => 'remote:4730/host',
    'remote:4730/check_results' => 'local:4730/check_results',
};

#################################################
# create worker
my $workers = {};
for my $conf (keys %{$config}) {
    my($server,$queue) = split/\//, $conf, 2;
    my $worker = $workers->{$server};
    unless( defined $worker) {
        $worker = Gearman::Worker->new(job_servers => [ $server ]);
        $workers->{$server} = $worker;
    }
    $worker->register_function($queue => sub { forward_job($config->{$conf}, @_) } );
}
my $clients = {};

# start all worker
my $threads = [];
for my $worker (values %{$workers}) {
    push @{$threads}, threads->create('worker', $worker);
}

# wait till worker finish (hopefully never)
for my $thr (@{$threads}) {
    $thr->join();
}
exit;

#################################################
sub worker {
    my $worker = shift;
    $worker->work while 1;
}

#################################################
sub forward_job {
    my($target,$job) = @_;
    my($server,$queue) = split/\//, $target, 2;

    print $job->handle." -> $target\n";

    my $client = $clients->{$server};
    unless( defined $client) {
        $client = Gearman::Client->new(job_servers => [ $server ]);
        $clients->{$server} = $client;
    }

    $client->dispatch_background($queue, $job->arg, { uniq => $job->handle });
    return;
}
