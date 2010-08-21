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
               [ --server=<host>:<port> ]

=head1 DESCRIPTION

This script prints out information about queues of a server

=head1 ARGUMENTS

script has the following arguments

=head2 help

  -h

display the help and exit

=head2 server

  --server=<host>:<port>

connect to this gearman server to print the queue. Can be used multiple times to add more server.

=head1 EXAMPLE

  ./queue_top.pl --server=localhost:4730

=head1 AUTHOR

Sven Nierlein, <sven@consol.de>

=cut


#################################################################
# parse cmd line arguments
my($opt_verbose, $opt_help, @opt_server);
Getopt::Long::Configure('no_ignore_case');
GetOptions (
    "h"               => \$opt_help,
    "v"               => \$opt_verbose,
    "server=s"        => \@opt_server,
);

if(defined $opt_help) {
    pod2usage( { -verbose => 1 } );
    exit 3;
}
if(scalar @opt_server == 0) {
    pod2usage( { -verbose => 1, -message => "please specify at least one gearman server ( --server=... ) to fetch statistics from." } );
    exit 3;
}


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
            my @functions = $session->status();
            print_data(\@functions);
        };
        if($@) {
            warn $@;
            eval { $session->shutdown('graceful') if defined $session; };
            undef $sessions->{$server};
        }
    }
    sleep(2);
}

# stop all sessions
for my $session (values %{$sessions}) {
    eval {
        $session->shutdown('graceful') if defined $session;
    };
}
exit;

#################################################################
sub print_data {
    my($data) = @_;;
    return unless defined $data;
    return unless @{$data} > 0;
    my $table = Text::TabularDisplay->new(keys %{$data->[0]});
    for my $row (@{$data}) {
        next if defined $row->{'functions'} and scalar @{$row->{'functions'}} == 0;
        next if defined $row->{'free'} and $row->{'free'} == 0 and $row->{'queue'} == 0 and $row->{'running'} == 0 and $row->{'busy'} == 0;
        my @data = values %{$row};
        my @row;
        for my $col (@data) {
           my $value;
           if(ref $col eq 'ARRAY') {
               $value = join(", ", @{$col});
           } else {
               $value = $col;
           }
           push @row, $value;
        }
        $table->add(@row);
    }
    print $table->render;
    print "\n";
}

#################################################################
sub set_sessions {
    for my $server (@opt_server) {
        next if defined $sessions->{$server};
        my($host,$port) = split/:/, $server, 2;
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
