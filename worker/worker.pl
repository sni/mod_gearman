#!/usr/bin/perl

use forks;
use strict;
use warnings;
use utf8;
use Gearman::Worker;
use Gearman::Client;
use Time::HiRes qw/gettimeofday tv_interval/;
use Data::Dumper;
use Getopt::Long;
use Pod::Usage;
use Carp;
use Sys::Hostname;

=head1 NAME

worker.pl

=head1 SYNOPSIS

./worker.pl [ -v ]
            [ -h ]
            [ -d ]
            [ -threads=10 ]
            [ --server=<host>:<port> ]
            [ --events ]
            [ --hosts ]
            [ --services ]
            [ --hostgroup=<name> ]
            [ --servicegroup=<name> ]

=head1 DESCRIPTION

This worker connects to your gearman server and executes jobs

=head1 ARGUMENTS

script has the following arguments

=head2 help

  -h

display the help and exit

=head2 verbose

  -v

enable verbose mode

=head2 debug

  -d

enable hostname in output

=head2 threads

  --threads=<number of workers>

creates this number of workers. Default is 1.

=head2 server

  --server=<host>:<port>

connect to this gearman server to fetch jobs. Can be used multiple times to add more server.

=head2 events

  --events

enable processing of eventhandler jobs

=head2 hosts

  --hosts

enable processing of host check jobs

=head2 services

  --services

enable processing of service check jobs

=head2 hostgroup

  --hostgroup=<name>

enable processing of checks for this hostgroup. Can be used multiple times to add more groups.

=head2 servicegroup

  --servicegroup=<name>

enable processing of checks for this servicegroup. Can be used multiple times to add more groups.

=head2 max_age

  --max_age=<seconds>

maximum age for scheduled checks before they get ignored, default is 120 seconds.

=head1 EXAMPLE

  ./worker.pl --server=localhost:4730 --events --services --hosts --hostgroup=hostgroup1 --hostgroup=hostgroup2 --servicegroup=dmz

=head1 AUTHOR

Sven Nierlein, <sven@consol.de>

=cut

############################################################
# parse cmd line arguments
my(@opt_verbose, $opt_help, $opt_debug, $opt_events, $opt_services, $opt_hosts, @opt_server, @opt_hostgroups, @opt_servicegroups, $opt_threads, $opt_maxage);
Getopt::Long::Configure('no_ignore_case');
GetOptions (
    "h"               => \$opt_help,
    "v"               => \@opt_verbose,
    "d"               => \$opt_debug,
    "server=s"        => \@opt_server,
    "events"          => \$opt_events,
    "hosts"           => \$opt_hosts,
    "services"        => \$opt_services,
    "hostgroups=s"    => \@opt_hostgroups,
    "servicegroups=s" => \@opt_servicegroups,
    "threads=i"       => \$opt_threads,
    "max_age=i"       => \$opt_maxage,
);

if(defined $opt_help) {
    pod2usage( { -verbose => 1 } );
    exit 3;
}
if(scalar @opt_server == 0) {
    pod2usage( { -verbose => 1, -message => "please specify at least one gearman server ( --server=... ) to fetch jobs from." } );
    exit 3;
}

my $opt_verbose = scalar @opt_verbose;
if(!$opt_services and !$opt_hosts and !$opt_events and scalar @opt_hostgroups == 0 and scalar @opt_servicegroups == 0) {
    $opt_events   = 1;
    $opt_services = 1;
    $opt_hosts    = 1;
}
_out("setting verbose level to ".$opt_verbose) if $opt_verbose;

#################################################
# defaults
$opt_threads     = 1   unless defined $opt_threads;
$opt_maxage      = 120 unless defined $opt_maxage;

#################################################
# settings
my $default_timeout = 60; # normally set by the check itself

#################################################
# debug
my $debug_workername = "";
if (defined $opt_debug){
    $debug_workername = hostname();
    $debug_workername = "$debug_workername - ";
}

#################################################
# split up possible defined csv servers
my @new_server;
for my $server (@opt_server) {
    my @serv = split/,/, $server;
    @new_server = (@new_server, @serv);
}
@opt_server = @new_server;

#################################################
# start the worker
if($opt_threads > 1) {
    for(my $nr = 0; $nr < $opt_threads; $nr++) {
        my $thread = threads->new( sub {
            start_worker($nr);
        });
    }
    $_->join foreach threads->list; #block until all threads done
}
else {
    start_worker(1);
}
exit 0;

#################################################

=head2 exec_handler

do the work

=cut
sub exec_handler {
    my $job  = shift;
    confess("no job") unless defined $job;

    my $t0   = [gettimeofday];
    my $data;
    eval { $data = _decode_data($job->arg) };
    if($@ or !defined $data or !defined $data->{'command_line'} or !defined $data->{'type'}){
        warn("got garbaged data: ".$@);
        return 1;
    }

    # check against max age
    my $latency = $data->{'latency'} || 0;
    my($start_time,$age);
    if(!defined $data->{'start_time'}) {
        $start_time = [gettimeofday];
    } else {
        $start_time = [split(/\./, $data->{'start_time'}, 2)];
    }
    $age = tv_interval( $start_time, $t0);
    if($age > $opt_maxage) {
        _out("max age reached for this job: ".$age." sec");
        return 1;
    }
    $latency += $age;

    _out(sprintf("got %s job - age %02f: %s %s", $data->{'type'}, $age, $data->{'host_name'} || '', $data->{'service_description'} || '')) if $opt_verbose;

    my $timeout       = $data->{'timeout'} || $default_timeout;
    my $early_timeout = 0;
    my($erg,$rc);
    eval {
        local $SIG{ALRM} = sub { die "alarm\n" };
        alarm $timeout;
        $erg = `$data->{'command_line'}`;
        $rc      = $?>>8;
        alarm 0;
    };
    if($@) {
        die unless $@ eq "alarm\n"; # propagate unexpected errors
        $early_timeout = 1;
        $rc            = 2;
        $erg       = sprintf("(%s Check Timed Out)", ucfirst($data->{'type'}));
        _out($data->{'type'}." job timed out after ".$timeout." seconds") if $opt_verbose;
    }

    my $t1      = [gettimeofday];
    my $elapsed = tv_interval( $t0, $t1);

    # eventhandler are finished at this point
    return 1 unless defined $data->{'host_name'};

    my $result = {
        host_name           => $data->{'host_name'},
        check_options       => $data->{'check_options'},
        scheduled_check     => $data->{'scheduled_check'},
        reschedule_check    => $data->{'reschedule_check'},
        start_time_tv_sec   => $start_time->[0],
        start_time_tv_usec  => $start_time->[1],
        early_timeout       => $early_timeout,
        exited_ok           => 1,
    };

    $erg =~ s/\n/\\n/gmx;
    $result->{finish_time_tv_sec}  = $t1->[0];
    $result->{finish_time_tv_usec} = $t1->[1];
    $result->{early_timeout}       = $early_timeout;
    $result->{return_code}         = $rc;
    $result->{output}              = $erg;
    $result->{latency}             = $latency;
    $result->{service_description} = $data->{'service_description'} if defined $data->{'service_description'};

    _out("finished job with rc ".$rc) if $opt_verbose;

    my $result_string = _build_result($result);
    _send_result($data->{'result_queue'}, $result_string);
    return 1;
}

#################################################

=head2 _build_result

return the output

=cut
sub _build_result {
    my $data = shift;
    my $service = "";
    $service    = "\nservice_description=".$data->{'service_description'} if defined $data->{'service_description'};
    my $result =<<EOT;
host_name=$data->{'host_name'}$service
check_options=$data->{'check_options'}
scheduled_check=$data->{'scheduled_check'}
reschedule_check=$data->{'reschedule_check'}
start_time=$data->{'start_time_tv_sec'}.$data->{'start_time_tv_usec'}
latency=$data->{'latency'}
early_timeout=$data->{'early_timeout'}
exited_ok=$data->{'exited_ok'}
finish_time=$data->{'finish_time_tv_sec'}.$data->{'finish_time_tv_usec'}
return_code=$data->{'return_code'}
output=$debug_workername$data->{'output'}

EOT

    _out("result:\n".$result) if $opt_verbose >= 3;

    return($result);
}

#################################################

=head2 _send_result

send the result

=cut
sub _send_result {
    my $queue = shift;
    my $data  = shift;
    die("got no result queue name") unless defined $queue;

    my $client = Gearman::Client->new;
    $client->job_servers(@opt_server);

    # add background job to send result back to core
    _out("sending result...\n") if $opt_verbose >= 1;
    $client->dispatch_background($queue => $data, {timeout => 60, retry_count => 0, try_timeout => 10});

    _out("finished sending result...\n") if $opt_verbose >= 1;

    return 1;
}

#################################################

=head2 start_worker

start the worker

=cut

sub start_worker {
    my $nr = shift;
    my $worker = Gearman::Worker->new;
    _out("connecting to: ".join(', ',@opt_server)) if $opt_verbose;
    $worker->job_servers(@opt_server);

    _register_function($worker, 'eventhandler')           if($opt_events);
    _register_function($worker, 'host')                   if($opt_hosts);
    _register_function($worker, 'service')                if($opt_services);
    _register_function($worker, \@opt_hostgroups,    'hostgroup_');
    _register_function($worker, \@opt_servicegroups, 'servicegroup_');

    while (1) {
        my $ret = $worker->work();
    }

    return;
}

#################################################
sub _register_function {
    my $worker   = shift;
    my $function = shift;
    my $prefix   = shift || '';

    my @functions;
    if(ref $function eq 'ARRAY') {
        @functions = @{$function};
    } else {
        push @functions, $function;
    }
    for my $func (@functions) {
        for my $f (split/,/, $func) {
            $worker->register_function($prefix.$f, 0, sub { return exec_handler(@_); });
            _out("registered for ".$prefix.$f." check jobs") if $opt_verbose;
        }
    }
    return;
}

#################################################
sub _out {
    my $msg = shift;
    chomp($msg);
    print "[".scalar(localtime)."][t".threads->tid."] ".$msg."\n";
    return;
}

#################################################
sub _decode_data {
    my $data   = shift;
    my $result = {};
    return unless defined $data;
    for my $line (split/\n/, $data) {
        my($key,$value) = split/=/,$line,2;
        next unless defined $key;
        next unless defined $value;
        $result->{$key} = $value;
    }
    return $result;
}
