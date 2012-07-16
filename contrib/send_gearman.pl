#!/usr/bin/perl

use strict;
use warnings;
use Nagios::Passive;
use Nagios::Passive::Gearman;
use Getopt::Long;

###################
# Vincent CANDEAU #
# Capensis - v1.0 #
###################

### Manage CLI options
my($server, $encryption, $key, $host, $service, $message, $returncode, $debug, $help);
my $params = GetOptions(
    "server=s"       => \$server,        # Serveur Addresse IP:PORT
    "e|encryption"   => \$encryption,    # Enable Encryption
    "k|key=s"        => \$key,           # Encryption key
    "h|host=s"       => \$host,          # HostName
    "s|service=s"    => \$service,       # Service Name
    "m|message=s"    => \$message,       # Output
    "r|returncode=i" => \$returncode,    # Return Code
    "d|debug"        => \$debug,         # Debug
    "help"           => \$help           # Display help
    );

### HELP
sub help {
    print("help:\n");
    print("usage : $0 [ --server <IP:PORT> ] [ -e / --encryption [ -k / --key <Key passphrase> ] ] [ -h / --host <hostname> ] [ -s / --service service name> ] [ -m / --message \"<output>\" ][ -r / --returncode <Code(0,1,2,3)> ] [ -d / --debug ] [ --help ]\n");
    print("\tserver\t\tServer IP:PORT (Default: IP: 127.0.0.1 / PORT: 4730)\n");
    print("\tencryption\tEnable / Disable Encryption\n");
    print("\tkey\t\tEncryption passphrase\n");
    print("\thost\t\tHost Name\n");
    print("\tservice\t\tService Name\n");
    print("\tmessage\t\tOutput message\n");
    print("\treturncode\tReturn Code\n");
    print("\tdebug\t\tEnable / Disable  debug\n");
    print("\thelp\t\tDisplay this message\n");
}

### MAIN
if( not $params ) {
    exit(1);
}
elsif ( defined($help) ) {
    &help();
    exit(0);
}
elsif ( !defined($host) || !defined($service) || !defined($message) || !defined($returncode) || ( defined($encryption) && ( !defined($key) ) ) ) {
    &help();
}
else {
    if( !defined($server) ) {
        $server = "127.0.0.1:4730";
    }
    else {
        if( not $server =~ /:/ ) {
            $server = $server . ":4730";
        }
    }

    if( defined($debug) ) {
        print "Server: " . $server . "\n";
        print "Encryption: " .(defined $encryption ? 'on' : 'off'). "\n";
        print "Key: " . $key . "\n" if defined $encryption;
        print "Host: " . $host . "\n";

        print "Service: " . $service . "\n";
        print "Message: " . $message . "\n";
        print "Code: " . $returncode . "\n";
    }

    my $gearman = Gearman::Client->new;
    $gearman->job_servers($server);

    my $nw;
    if( !defined($key) ) {
        $nw = Nagios::Passive->create(
            gearman             => $gearman,
            service_description => $service,
            check_name          => $service,
            host_name           => $host,
            return_code         => $returncode,
            output              => $message
        );
    }
    else {
        $nw = Nagios::Passive->create(
            gearman             => $gearman,
            key                 => $key,
            service_description => $service,
            check_name          => $service,
            host_name           => $host,
            return_code         => $returncode,
            output              => $message
        );
    }
    my $job = $nw->submit;
    print "submitted job: ".$job."\n" if defined $debug;
}
