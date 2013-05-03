#!/usr/bin/perl


=head1 NAME

dump_queue.pl

=head1 SYNOPSIS

./send_command.pl   [ -v ]
                    [ -h ]
                    [ -H hostname:port ]
                    [ -q queue ]
                    [ -p password ]
                    command

=head1 DESCRIPTION

This script connects to your gearman server and sends a command for a given queue

=head1 ARGUMENTS

script has the following arguments

=head2 help

  -h

display the help and exit

=head2 verbose

  -v

enable verbose mode

=head2 hostname

  -H hostname:port

hostname to connect to. Defaults to localhost:4730

=head2 queue

  -q queue

which queue to dump

=head2 timeout

  -t timeout

Timeout for this command. Defaults to 10sec

=head2 password

  -p password

password to encrypt data

=head1 EXAMPLE

  ./send_command.pl -H localhost:4730 -q service pwd

=head1 AUTHOR

2013, Sven Nierlein, <sven.nierlein@consol.de>

=cut

use warnings;
use strict;
use Getopt::Long;
use Data::Dumper;
use Pod::Usage;
use Gearman::Client;
use Gearman::Worker;
use MIME::Base64;
use Crypt::Rijndael;

################################################################################
# read options
my($opt_h, $opt_v, $opt_H, $opt_q, $opt_c, $opt_p, $opt_t);
Getopt::Long::Configure('no_ignore_case');
if(!GetOptions(
    "v"   => \$opt_v,
    "h"   => \$opt_h,
    "H=s" => \$opt_H,
    "q=s" => \$opt_q,
    "p=s" => \$opt_q,
    "t=i" => \$opt_t,
    "v"   => \$opt_v,
    "<>"  => sub { $opt_c = $_[0] },
)) {
    pod2usage( { -verbose => 1, -message => "error in options" } );
    exit 3;
}
my $verbose = $opt_v;

if(defined $opt_h) {
    pod2usage( { -verbose => 1 } );
    exit 3;
}
if(!defined $opt_q) {
    pod2usage( { -verbose => 1, -message => "please specify at least one queue." } );
    exit 3;
}
$opt_H = "localhost:4730" unless defined $opt_H;
$opt_t = 10 unless defined $opt_t;
alarm($opt_t);

################################################################################
# use encryption
my $cypher;
if(defined $opt_p) {
    my $key = substr($opt_p,0,32) . chr(0) x ( 32 - length( $opt_p ) );
    $cypher = Crypt::Rijndael->new( $key, Crypt::Rijndael::MODE_ECB() );
}

################################################################################
# create gearman worker
my $client = Gearman::Client->new;
$client->job_servers($opt_H);

################################################################################
# send job
print "command: ".$opt_c."\n" if $verbose;
my $result_queue = 'tmp_send_command_results';
my $data = sprintf("type=host\nresult_queue=%s\nhost_name=%s\nstart_time=%i.0\nnext_check=%i.0\ntimeout=%d\ncore_time=%i.%i\ncommand_line=%s\n\n\n",
                    $result_queue,
                    'fake host',
                    time()+600,
                    time()+600,
                    $opt_t - 10,
                    time(),
                    0,
                    $opt_c,
                  );
print "sending: ".$data."\n" if $verbose;
$data = encode_base64($data);
print "base64: ".$data."\n" if $verbose;
if(defined $cypher) {
    $data = $cypher->encrypt($data);
    print "encrypted: ".$data."\n" if $verbose;
}
$client->do_task($opt_q, $data);

################################################################################
# wait for result
$SIG{ALRM} = sub { exit 0; };
my $worker = Gearman::Worker->new;
print "connecting to $opt_H\n" if $opt_v;
$worker->job_servers($opt_H);
$worker->register_function($result_queue => sub { return dump_job(@_); });
print "registered queue: $result_queue\n" if $verbose;
print "starting to work\n" if $verbose;
$worker->work;
print "work finished\n" if $verbose;

exit;

################################################################################
# print job via Data::Dumper
sub dump_job {
    my $job  = shift;
    my $data = decode_base64($job->arg);
    if(defined $cypher) {
        $data = $cypher->decrypt($data);
    }
    print "result:\n";
    print $data;
    alarm(1);
    return 1;
}
