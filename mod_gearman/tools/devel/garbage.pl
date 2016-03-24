#!/usr/bin/perl

# This is to insert junk into the queue specified in command line to 'rainbow' test it :)

use Gearman::Client;
use String::Random qw(random_string);
use Sys::Hostname;
use Getopt::Long;
use Pod::Usage;
use strict;
use warnings;
use utf8;

=head1 NAME

garbage.pl

=head1 SYNOPSIS

./garbage.pl [ -v ]
            [ -h ]
            [ -c ]
            [ --queue=<name> ]
            [ --loops=<number> ]
            [ --server=<host>:<port> ]
            [ --hosts=name1,name2,name3... ]
            [ --services=name1,name2,name3... ]
            [ --hostprefix=<name> ]
            [ --serviceprefx=<name> ]

=head1 DESCRIPTION

This script connects to your job server and inserts junk into the specified queue. If no queue's given, it inserts into the queue check_results
There is no logic to tie hostname or service to the correct fields, its there just to harazz(to make the core crash!)

=head1 ARGUMENTS

script has the following arguments

=head2 help

  -h

display the help and exit

=head2 verbose

  -v

enable verbose mode

=head2 loops

  --loops=<number>

sets the number of loops(inserts) to do, default is 200

=head2 queue

  --queue=<queue>

specifies the queue to insert the junk into, default is check_results

=head2 confuse

  -c

try to confuse with hostname in the junk sent to the job server

=head2 services

  --services=<name>

Servicename to use(no need to specify), if several, use a , to separate.

=head2 hosts

  --hosts=<name>

Hostname to use(no need to specify), if several, use a , to separate.

=head2 hostprefix

  --hostprefix=<name>

If you want to specify a prefix to be appended to ALL hostnames, specify with this switch.

=head2 serviceprefix

  --serviceprefix=<name>

If you want to specify a prefix to be appended to ALL servicenames, specify with this switch.

=head1 EXAMPLE

  ./garbage.pl --server=localhost:4730 --loops=500 -c

=head1 AUTHOR

Rune "TheFlyingCorpse" Darrud, <theflyingcorpse@gmail.com>

=cut

############################################################
# parse cmd line arguments
my(@opt_verbose, $opt_help, $opt_confuse, @opt_server, $opt_loops, $opt_queue, @opt_services, @opt_hosts, $opt_hostprefix, $opt_serviceprefix);
Getopt::Long::Configure('no_ignore_case');
GetOptions (
    "h"               => \$opt_help,
    "v"               => \@opt_verbose,
    "c"               => \$opt_confuse,
    "server=s"        => \@opt_server,
    "loops=i"         => \$opt_loops,
    "queue=s"         => \$opt_queue,
    "services=s"      => \@opt_services,
    "hosts=s"         => \@opt_hosts,
    "hostprefix=s"    => \$opt_hostprefix,
    "serviceprefix=s" => \$opt_serviceprefix,
);

if(defined $opt_help) {
    pod2usage( { -verbose => 1 } );
    exit 3;
}
if(scalar @opt_server == 0) {
    pod2usage( { -verbose => 1, -message => "please specify at least one gearman server ( --server=... ) to insert jobs to." } );
    exit 3;
}
my $opt_verbose = scalar @opt_verbose;
if(!$opt_confuse and !$opt_loops and !$opt_queue) {
    $opt_confuse   = 1;
    $opt_loops     = 200;
    $opt_queue     = 'check_results';
}

_out("setting verbose level to ".$opt_verbose) if $opt_verbose >= 1;

#################################################
# Settings / General parameters
my $hostname = hostname(); # Can be used to confuse?

#################################################
# Defaults
$opt_loops = 200 unless defined $opt_loops;
$opt_queue = 'check_results' unless defined $opt_queue;

_out("$opt_loops is the target insertions") if $opt_verbose >= 1;
_out("$opt_queue is the target queue") if $opt_verbose >= 1;

#################################################
# split up possible defined csv arguments
my @new_server;
for my $server (@opt_server) {
    my @serv = split/,/, $server;
    @new_server = (@new_server, @serv);
}
@opt_server = @new_server;

_out("@opt_server are the target job servers") if $opt_verbose >= 1;


if (scalar @opt_services > 0){
    my @new_service;
    for my $service (@opt_services) {
        my @desc = split/,/, $service;
        @new_service = (@new_service, @desc);
    }
    @opt_services = @new_service;

    _out("@opt_services are the services that will be used") if $opt_verbose >= 1;
}

if (scalar @opt_hosts > 0){
    my @new_hosts;
    for my $host (@opt_hosts) {
        my @hosts = split/,/, $host;
        @new_hosts = (@new_hosts, @hosts);
    }
    @opt_hosts = @new_hosts;

    _out("@opt_hosts are the random servers that will be used") if $opt_verbose >= 1;
}

#################################################
# Use Gearman::Client
my $client = Gearman::Client->new;
my $rands  = new String::Random;

#################################################
# Set the targets for the junk
$client->job_servers(@opt_server);

#################################################
# Now insert junk
my $now = time();
my $end = $now+1;
for(my $i = 0; $i < $opt_loops; $i++) {
    my $temp_hostname     = _randomnessHost();
    my $temp_servicename  = _randomnessService();
    my $temp_output       = _randomOutput()."|"._randomperfData();
    my $load;
    if($i%2==0) {
        $load             = "host_name=".$temp_hostname."\nservice_description=".$temp_servicename."\nstart_time=$now.0\nfinish_time=$end.0\noutput=".$temp_output."\n";
    } else {
        $load             = "host_name=".$temp_hostname."\nstart_time=$now.0\nfinish_time=$end.0\noutput=".$temp_output."\n";
    }
    my $handle         = $client->dispatch_background($opt_queue => $load);
    _out("inserted $i out of $opt_loops, handle: $handle data: $load") if $opt_verbose >= 1; 
}

_out("finished inserting processing $opt_loops insertions of random junk into $opt_queue") if $opt_verbose >= 1;


#################################################
sub _out {
    my $msg = shift;
    chomp($msg);
    print "[".scalar(localtime)."] ".$msg."\n";
    return;
}

#################################################
sub _randomnessHost {
    my $random_hostname; 
    # Add another n to the others, if you want to see 3 random digits instead of just 2, optional to have the c at the back(in some areas its used as a zone indicator)
    if (scalar @opt_hosts > 0){
        $random_hostname = _randomselect(1);
    } else {
        my $nr = int(rand(2000)); # generate random string with random size up to 200k chars
        $random_hostname = $rands ->randregex(".{$nr}");
    }
    $random_hostname = $opt_hostprefix.$random_hostname if defined $opt_hostprefix;
    _out("randomness generated the following \"hostname\": $random_hostname") if $opt_verbose >= 3;
    return $random_hostname;
}

#################################################
sub _randomnessService {
    my $random_servicename;
    if (scalar @opt_services > 0){
        $random_servicename = _randomselect(2);
    } else {
        my $nr = int(rand(2000)); # generate random string with random size up to 200k chars
        $random_servicename = $rands ->randregex(".{$nr}");
    }
    $random_servicename = $opt_serviceprefix.$random_servicename if defined $opt_serviceprefix;
    _out("randomness generated the following \"service\": $random_servicename") if $opt_verbose >= 3;
    return $random_servicename;

}

#################################################
sub _randomOutput {
    my $random_checkcommand;
    $random_checkcommand = $rands ->randpattern("cccccccccccccccccccccccccccCCCC");
    _out("randomness generated the following \"check command\": $random_checkcommand") if $opt_verbose >= 3;
    return $random_checkcommand;
}

#################################################
sub _randomperfData {
    my $min   = $rands ->randpattern("nnnn");
    my $max   = $rands ->randpattern("nnnn");
    my $warn  = $rands ->randpattern("nnnn");
    my $crit  = $rands ->randpattern("nnnn");
    my $value = $rands ->randpattern("nnnn");
    my $random_perfData = "data=$value;$max;$min;$warn;$crit";
    _out("randomness generated the following \"perfdata\": $random_perfData") if $opt_verbose >= 3;
    return $random_perfData;
}


#################################################
sub _randomselect {
    my $choosing = shift;
    chomp($choosing);
    my @array;
    my $index;
    my $element;
    if ($choosing==1){
        @array = @opt_hosts;
    } elsif ($choosing==2){
        @array = @opt_services;
    }
    srand;
    $index = rand @array;
    $element = $array[$index];
    return $element;
}
