#!/usr/bin/perl


=head1 NAME

dump_queue.pl

=head1 SYNOPSIS

./dump_queue.pl [ -v ]
                [ -h ]
                [ -H=hostname:port ]
                [ -q=queue ]
                [ -c=configfile ]

=head1 DESCRIPTION

This worker connects to your gearman server and dumps data for given queues

=head1 ARGUMENTS

script has the following arguments

=head2 help

  -h

display the help and exit

=head2 verbose

  -v

enable verbose mode

=head2 hostname

  -H=hostname:port

hostname to connect to. Defaults to localhost:4731

=head2 queue

  -q=queue

which queue to dump

=head2 configfile

  -c=configfile

read config from config file

=head1 EXAMPLE

  ./dump_queue.pl -H localhost:4731 -q log

=head1 AUTHOR

2011, Sven Nierlein, <sven.nierlein@consol.de>

=cut

use warnings;
use strict;
use Getopt::Long;
use Data::Dumper;
use Pod::Usage;
use Gearman::Worker;
use MIME::Base64;
use Crypt::Rijndael;
use JSON::XS;

################################################################################
# read options
my($opt_h, $opt_v, $opt_H, @opt_q, $opt_p);
Getopt::Long::Configure('no_ignore_case');
if(!GetOptions(
    "v"   => \$opt_v,
    "h"   => \$opt_h,
    "H=s" => \$opt_H,
    "q=s" => \@opt_q,
    "p=s" => \$opt_p,
    "v"   => \$opt_v,
)) {
    pod2usage( { -verbose => 1, -message => "error in options" } );
    exit 3;
}

if(defined $opt_h) {
    pod2usage( { -verbose => 1 } );
    exit 3;
}
if(scalar @opt_q == 0) {
    pod2usage( { -verbose => 1, -message => "please specify at least one queue." } );
    exit 3;
}
$opt_H = "localhost:4730" unless defined $opt_H;

################################################################################
# use encryption
my $cypher;
if(defined $opt_p) {
    my $key = substr($opt_p,0,32) . chr(0) x ( 32 - length( $opt_p ) );
    $cypher = Crypt::Rijndael->new( $key, Crypt::Rijndael::MODE_ECB() );
}

################################################################################
# create gearman worker
my $worker = Gearman::Worker->new;
$worker->job_servers($opt_H);
for my $queue (@opt_q) {
    $worker->register_function($queue, 2, sub { return dump_job(@_); });
    print "registered queue: $queue\n" if $opt_v;
}
my $req = 0;
print "starting to work\n" if $opt_v;
$worker->work();
print "work finished\n" if $opt_v;

################################################################################
# print job via Data::Dumper
sub dump_job {
    my $job  = shift;
    my $data = decode_base64($job->arg);
    if(defined $cypher) {
        $data = $cypher->decrypt($data);
    }
    print "###################\n";
    print Dumper($data);
    $data = decode_json($data);
    #print Dumper($data);
    return 1;
}