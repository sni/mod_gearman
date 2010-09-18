#!/usr/bin/perl

use strict;
use warnings;
use Data::Dumper;
use Carp;
use Net::Telnet::Gearman;
use Text::TabularDisplay;
use Getopt::Long;
use Pod::Usage;
use Carp;

=head1 NAME

queue_top.pl

=head1 SYNOPSIS

./queue_top.pl [ -h ]
               [ -q ]
               [ --server=<host>:<port> ]

=head1 DESCRIPTION

This script prints out information about queues of a server

=head1 ARGUMENTS

script has the following arguments

=head2 help

  -h

display the help and exit

=head2 quiet

  -q

dont show empty queues without worker

=head2 server

  --server=<host>:<port>

connect to this gearman server to print the queue. Can be used multiple times to add more server.

=head2 interval

  -i=<interval>

refresh page at this interval (seconds)

=head1 EXAMPLE

  ./queue_top.pl --server=localhost:4730 -i 2

=head1 AUTHOR

Sven Nierlein, <sven@consol.de>

=cut


#################################################################
# parse cmd line arguments
my($opt_verbose, $opt_help, $opt_interval, @opt_server, $opt_quiet);
Getopt::Long::Configure('no_ignore_case');
GetOptions (
    "h"               => \$opt_help,
    "v"               => \$opt_verbose,
    "q"               => \$opt_quiet,
    "server=s"        => \@opt_server,
    "i=i"             => \$opt_interval,
);

if(defined $opt_help) {
    pod2usage( { -verbose => 1 } );
    exit 3;
}
if(scalar @opt_server == 0) {
    pod2usage( { -verbose => 1, -message => "please specify at least one gearman server ( --server=... ) to fetch statistics from." } );
    exit 3;
}

$opt_interval = 1 unless defined $opt_interval;

#################################################################
# make first connection outside eval, so if the server does not exist
# we immediatly exit
my $sessions;
set_sessions();

#################################################################
# start our top loop
while(1) {
    system("clear");
    print scalar localtime;
    print "\n";

    set_sessions();
    for my $server (@opt_server) {
        next unless defined $sessions->{$server};
        my $session = $sessions->{$server};
        eval {
            # print queues
            print "Functions on ".$server."\n";
            my @queue = $session->status();
            print_queue(\@queue);
        };
        if($@) {
            warn $@;
            eval { $session->shutdown('graceful') if defined $session; };
            undef $sessions->{$server};
        }
    }
    sleep($opt_interval);
}

# stop all sessions
for my $session (values %{$sessions}) {
    eval {
        $session->shutdown('graceful') if defined $session;
    };
}
exit;

#################################################################
sub print_queue {
    my($data) = @_;
    return unless defined $data;
    return unless @{$data} > 0;
    my $table = Text::TabularDisplay->new('Queue Name', 'Worker Available', 'Jobs Waiting', 'Jobs Running');
    for my $row (@{$data}) {

        # shall we skip empty queues?
        next if defined $opt_quiet and ($row->{'name'} eq "dummy");
        next if defined $opt_quiet and ($row->{'queue'} == 0 and $row->{'running'} == 0);

        my $result = [$row->{'name'}, $row->{'running'}, $row->{'queue'}-$row->{'busy'}, $row->{'busy'}];
        $table->add(@{$result});
    }
    print $table->render;
    print "\n";
}

#################################################################
sub set_sessions {
    for my $server (@opt_server) {
        next if defined $sessions->{$server};
        my($host,$port) = split/:/, $server, 2;
        $port = 4730 unless defined $port;
        eval {
            my $session = Net::Telnet::Gearman->new(
                Host => $host,
                Port => $port,
            );
            $sessions ->{$server} = $session;
        };
        warn $@ if $@;
    }
    return;
}
