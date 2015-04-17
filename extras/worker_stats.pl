#!/usr/bin/perl -w
#Created by erik.mathis@tritondigital.com 8/2014
#to be used to report the stats the mod_gearman_worker. debug=1 must be set in mod_gearman_worker config

use strict;
use File::Tail;
use Sys::Syslog;
use Sys::Hostname;

my $scriptpid="/var/run/nrpe/worker_stats.pid";
my $workerconf="/etc/mod_gearman/mod_gearman_worker.conf";
my $logfile="/var/log/mod_gearman/mod_gearman_worker.log";
my $pidfile="/var/run/mod_gearman/mod_gearman_worker.pid";
my $pstree="pstree -Apc";
my $sendinterval=60; #seconds #purges the mem if a report hasnt been generated
my $sendgm="/usr/bin/send_gearman";
my $VERSION=1.05;

$SIG{HUP}  = \&mk_report; #write nagios reults to $scratchfile
$SIG{TERM} = \&death; #exit this script

#name of this script
my $process = $0;
$process =~ s/^.*\/|\.pl//g;

#hold the info collected
my %stats;

&daemonize;

my $mypid=$$;
open(OUT, ">$scriptpid") or death("Cant write pidfile $scriptpid");
print OUT $mypid;
close OUT;

my $hostname=hostname;

logit("$process starting up on $hostname :: $VERSION");

#used tell when $purgetime has been reached.
my $start=time;

#Start the tail also the main loop
my $ref=tie *FH,"File::Tail",(name=>$logfile, maxinterval=>100,resetafter=>300) or death("Failed to open $logfile $!");
while (<FH>) {
    my $line=$_;
    chomp($line);
    
    #[2014-08-15 12:20:23][29384][DEBUG] got service job: web-03.tsso.internal - NGINX Stats
    my ($date,$pid,$mode,$data)=split(/\]/,$line);   
    
    if ($data =~ /got service job/) {
        if (!$stats{job_service}) { $stats{job_service}=1 }
        else { $stats{job_service}++; }
    }
    
    if ($data =~ /got host job/) {
        if (!$stats{job_host}) { $stats{job_host}=1 }
        else { $stats{job_host}++; }        
    }
    
    if ($data =~ /child started with pid/) {
        if (!$stats{newpid}) { $stats{newpid}=1 }
        else { $stats{newpid}++; }        
    }
    
    if ($data =~ /restarting all workers/) {
        if (!$stats{stale}) { $stats{stale}=1 }
        else { $stats{stale}++; }        
    }
    
    if ($data =~ /\[ERROR\]/) {
        if (!$stats{error}) { $stats{error}=1 }
        else { $stats{error}++; }        
    }
    
    if (!$stats{total}) { $stats{total}=1 }
    else { $stats{total}++; }
        
    my $now=time;
    if (($now-$start) >= $sendinterval ) {
        logit("Sending $sendinterval seconds Report");
        &mk_report;
    }
}

#Send of the script
&death;

#build report based off the data collected. json to syslog and nagios format to $scratchfile
sub mk_report {
    my $last=shift;
    
    my $svc=0;
    my $host=0;
    my $stale=0;
    my $new=0;
    my $error=0;
    my $total=0;
 
    $svc+=$stats{job_service} if $stats{job_service};
    $host+=$stats{job_host} if $stats{job_host};
    $stale+=$stats{stale} if $stats{stale};
    $new+=$stats{newpid} if $stats{newpid};
    $error+=$stats{error} if $stats{error};
    $total+=$stats{total};
    
    #Get process info
    my $psinfo=&psInfo;
        
    logit(" { \"service\":$svc, \"host\":$host, \"error\": $error, \"stale\":$stale, \"new\":$new, \"total_actions\":$total, \"total_workers\":$psinfo->{total}, \"idle_workers\":$psinfo->{idle}, \"active_workers\":$psinfo->{active} }");
    
    #Passivly send the results back to the nagios
    my $now=time;
    my $p=sprintf("%.0f", $now-$start);
    my $cmd="$sendgm --config=\"$workerconf\" --host=\"$hostname\" --service=\"Worker Stats\" --message=\'OK: $process age $p seconds|service=$svc.00; host=$host.00; errors=$error.00; stale=$stale.00; new=$new.00; total_actions=$total.00; total_workers=$psinfo->{total}.00; idle_workers=$psinfo->{idle}.00; active_workers=$psinfo->{active}.00\n\'";

    open(OUT, "$cmd|") or death("Cant run command $cmd :: $!");
    close OUT;
    
    #We got our numbers now its time for mem cleanup
    &purge;    
}

#build report on the _workers
sub psInfo {
    my $pid;
    open(IN,$pidfile) or death("Cant open $pidfile: $!");
    $pid=<IN>;
    close IN;
    chomp($pid);
    
    my $ttlwrk=0;
    my $actwrk=0;
    my $idlewrk=0;

    my $ps="$pstree $pid";

    open(CMD, "$ps|") or death("Cant run $ps: $?");
    while (<CMD>) {
        chomp;
	my $line=$_;
        my @things;
        @things=split(/---/, $line);
        if (scalar(@things) > 1 ) { $ttlwrk++; $actwrk++; } #value are only 1 or 3,4, 3,4 means its forked and running the job 1 is idle
        else { $idlewrk++; $ttlwrk++;} # everything lese is idle
    }
    close CMD;
    #return the results
    return { total=>$ttlwrk, active=>$actwrk, idle=>$idlewrk };
}

#Exit the script with some class
sub death {
    my $mess=shift;
    logit("ERROR: $mess ","error") if $mess;
    logit("Gracefully shutting down");
    unlink $scriptpid;
    close FH;    
    exit 0;
}

#mem cleanup
sub purge {
    %stats=();
    my $now=time;
    $start=$now; #reset the counter
}

#Send things to syslog like a good script should do.
sub logit {
    my $message = $_[0];
    my $priority = "info";
    $priority=$_[1] if $_[1];
    $message =~ s/\t/ /g;
    
    Sys::Syslog::setlogsock( 'unix' );
    Sys::Syslog::openlog( $process, 'cons,pid', 'user' );
    Sys::Syslog::syslog( "info", "%s\n", $message );
    Sys::Syslog::closelog();
}

#Run this script as a daemon
sub daemonize {
    chdir '/' or die "Can't chdir to /: $!";
    open STDIN, '/dev/null' or die "Can't read /dev/null: $!";
    open STDOUT, ">/dev/null";
    open STDERR, '>&STDOUT' or die "Can't dump to stdout: $!";

    # fork and exit parent
    my $pid = fork();
    exit if $pid;
    die "Couldn't fork: $!" unless defined ($pid);
    POSIX::setsid() || die ("$0 can't start a new session: $!");
}
