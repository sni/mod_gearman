#!/usr/bin/perl
# vim: expandtab:ts=4:sw=4:syntax=perl

=pod

=head1 NAME

gearman_proxy - proxy for gearman jobs

=head1 SYNOPSIS

gearman_proxy [options]

Options:

    'c|config'      defines the config file
    'l|log'         defines the logfile
    'd|debug'       enable debug output
    'h'             this help message

=head1 DESCRIPTION

This script redirects jobs from one gearmand server to another gearmand server.

=head1 OPTIONS

=item [B<--config> I<path>]

Specifies the path to the configfile.

=item [B<--log> I<path>]

Specifies the path to the logfile.

=item [B<--debug>]

Enable debug logging.

=head1 EXAMPLES


=cut

use warnings;
use strict;
use Gearman::Worker;
use Gearman::Client;
use threads;
use Pod::Usage;
use Getopt::Long;
use Data::Dumper;
use Socket qw(IPPROTO_TCP SOL_SOCKET SO_KEEPALIVE TCP_KEEPIDLE TCP_KEEPINTVL TCP_KEEPCNT);

our $pidFile;
our $logFile;
our $queues;
our $debug;

my $configFile;
my $help;
my $cfgFiles = [
    '~/.gearman_proxy',
    '/etc/mod-gearman/gearman_proxy.cfg',
];
if(defined $ENV{OMD_ROOT}) {
    push @{$cfgFiles}, $ENV{OMD_ROOT}.'/etc/mod-gearman/proxy.cfg';
}
GetOptions ('p|pid=s'    => \$pidFile,
            'l|log=s'    => \$logFile,
            'c|config=s' => \$configFile,
            'd|debug'    => \$debug,
            'h'          => \$help,
);
pod2usage(-exitval => 1) if $help;
if($configFile) {
    die("$configFile: $!") unless -r $configFile;
    $cfgFiles = [ $configFile ];
}

for my $cfgFile (@{$cfgFiles}) {
    ($cfgFile) = glob($cfgFile);
    out("looking for config file in ".$cfgFile) if $debug;
    next unless defined $cfgFile;
    next unless -f $cfgFile;

    out("reading config file ".$cfgFile) if $debug;
    do "$cfgFile";
    last;
}

my $listOfVariables = {
    'pidFile'    => $pidFile,
    'logFile'    => $logFile,
    'debug'      => $debug,
    'queues'     => $queues,
    'config'     => $configFile,
};
out('starting...');
out('startparam:');
out($listOfVariables);

if(!defined $queues or scalar keys %{$queues} == 0) {
    out('ERROR: no queues set!');
    exit 1;
}

#################################################
# save pid file
if($pidFile) {
    open(my $fhpid, ">", $pidFile) or die "open $pidFile failed: ".$!;
    print $fhpid $$;
    close($fhpid);
}

#################################################
# create worker
my $workers = {};
for my $conf (keys %{$queues}) {
    my($server,$queue) = split/\//, $conf, 2;
    $workers->{$server} = [] unless defined $workers->{$server};
    push @{$workers->{$server}}, { from => $queue, to => $queues->{$conf} };
}
my $clients = {};

# start all worker
my $threads = [];
for my $server (keys %{$workers}) {
    push @{$threads}, threads->create('worker', $server, $workers->{$server});
}

# wait till worker finish (hopefully never)
for my $thr (@{$threads}) {
    $thr->join();
}
unlink($pidFile) if $pidFile;
exit;

#################################################
# SUBS
#################################################
sub worker {
    my($server, $queues) = @_;

    while(1) {
        my $worker = Gearman::Worker->new(job_servers => [ $server ]);
        for my $queue (@{$queues}) {
            $worker->register_function($queue->{'from'} => sub { forward_job($queue->{'to'}, @_) } );
        }

        _enable_tcp_keepalive($worker);

        while(1) {
            $worker->work;
        }
    }
}

#################################################
sub forward_job {
    my($target,$job) = @_;
    my($server,$queue) = split/\//, $target, 2;

    out($job->handle." -> ".$target) if $debug;

    my $client = $clients->{$server};
    unless( defined $client) {
        $client = Gearman::Client->new(job_servers => [ $server ]);
        $clients->{$server} = $client;
        _enable_tcp_keepalive($client);
    }

    $client->dispatch_background($queue, $job->arg, { uniq => $job->handle });
    return;
}

#################################################
sub _enable_tcp_keepalive {
    my($gearman_obj) = @_;

    # set tcp keepalive for our worker
    if($gearman_obj->{'sock_cache'}) {
        for my $sock (values %{$gearman_obj->{'sock_cache'}}) {
            setsockopt($sock, SOL_SOCKET,  SO_KEEPALIVE,   1);
            setsockopt($sock, IPPROTO_TCP, TCP_KEEPIDLE,  10); # The time (in seconds) the connection needs to remain idle before TCP starts sending keepalive probes
            setsockopt($sock, IPPROTO_TCP, TCP_KEEPINTVL,  5); # The time (in seconds) between individual keepalive probes.
            setsockopt($sock, IPPROTO_TCP, TCP_KEEPCNT,    3); # The maximum number of keepalive probes TCP should send before dropping the connection
        }
    }
    return;
}

#################################################
sub out {
    my($txt) = @_;
    return unless defined $txt;
    if(ref $txt) {
        return(out(Dumper($txt)));
    }
    chomp($txt);
    my @txt = split/\n/,$txt;
    if($logFile) {
        open (my $fh, ">>", $logFile) or die "open $logFile failed: ".$!;
        for my $t (@txt)  {
            print $fh localtime(time)." ".$t,"\n";
        }
        close ($fh);
    } else {
        for my $t (@txt)  {
            print localtime(time)." ".$t."\n";
        }
    }
    return;
}