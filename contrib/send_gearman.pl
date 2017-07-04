#!/usr/bin/perl

use strict;
use warnings;
use Gearman::Client;
use Crypt::Rijndael;
use MIME::Base64;
use Getopt::Long;

###################
# Vincent CANDEAU #
# Capensis - v1.0 #
###################

### Manage CLI options
my($server, $encryption, $key, $host, $service, $message, $returncode, $queue, $debug, $help);
my $params = GetOptions(
    "server=s"       => \$server,        # Serveur Addresse IP:PORT,IP:PORT
    "e|encryption"   => \$encryption,    # Enable Encryption
    "k|key=s"        => \$key,           # Encryption key
    "h|host=s"       => \$host,          # HostName
    "s|service=s"    => \$service,       # Service Name
    "m|message=s"    => \$message,       # Output
    "r|returncode=i" => \$returncode,    # Return Code
    "d|debug"        => \$debug,         # Debug
    "q|queue"        => \$queue,         # Queue
    "help"           => \$help           # Display help
    );

### HELP
sub help {
    print("help:\n");
    print("usage : $0 [ --server <IP[:PORT],IP[:PORT]> ] [ -e / --encryption [ -k / --key <Key passphrase> ] ] [ -h / --host <hostname> ] [ -s / --service service name> ] [ -m / --message \"<output>\" ][ -r / --returncode <Code(0,1,2,3)> ] [ -q / --queue <result_queue> ] [ -d / --debug ] [ --help ]\n");
    print("\tserver\t\tServer IP:PORT (Default: IP: 127.0.0.1 / PORT: 4730)\n");
    print("\tencryption\tEnable / Disable Encryption\n");
    print("\tkey\t\tEncryption passphrase\n");
    print("\thost\t\tHost Name\n");
    print("\tservice\t\tService Name\n");
    print("\tmessage\t\tOutput message\n");
    print("\treturncode\tReturn Code\n");
    print("\tqueue\t\tResult queue (Default: check_results)\n");
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
    exit(0);
}

my @server;
if ( $server =~ /,/ ) {
    foreach my $adr ( split(',', $server) ) {
        if ( not $adr  =~ /:/ ) {
            $adr = $adr.":4730";
        }
        push( @server, $adr );
    }
} elsif ( ! defined($server) ) {
    $server = "127.0.0.1:4730";
    push( @server, $server );
} else {
    if ( not $server =~ /:/ ){
        $server = $server.":4730";
    }
    push( @server, $server );
}

if ( defined($debug) ){
    print "Server: ".join(',',@server)."\n";
    print "Encryption: " .(defined $encryption ? 'on' : 'off'). "\n";
    print "Key: " . $key . "\n" if defined $encryption;
    print "Host: " . $host . "\n";

    print "Service: " . $service . "\n";
    print "Message: " . $message . "\n";
    print "Code: " . $returncode . "\n";
}

my $gearman = Gearman::Client->new;
$gearman->job_servers($server);
my $job = _submit();
print "submitted job: ".$job."\n" if defined $debug;
exit;

################################################################
# shamelessly copied from Nagios::Passive::Gearman
sub _to_string {
    my $template = << 'EOT';
type=%s
host_name=%s%s
start_time=%i.%i
finish_time=%i.%i
latency=%i.%i
return_code=%i
output=%s
EOT
    my $result = sprintf $template,
        'passive',
        $host,
        (defined $service
            ? sprintf "\nservice_description=%s", $service
            : '' ),
        time,0,
        time,0,
        0,0,
        $returncode,
        $message;
    return $result;
}

################################################################
sub _encrypted_string {
    my $payload = _to_string();
    my $crypt = Crypt::Rijndael->new(
        _null_padding($key,32,'e'),
        Crypt::Rijndael::MODE_ECB() # :-(
    );
    $payload = _null_padding($payload,32,'e');
    $crypt->encrypt($payload);
}

################################################################
sub _submit {
    my $payload = (defined $key && $key ne '')
        ? _encrypted_string()
        : _to_string();
    $queue = 'check_results' unless $queue;
    $gearman->dispatch_background($queue, encode_base64($payload))
        or die("submitting job failed");
}

################################################################
# Thanks to Crypt::CBC
sub _null_padding {
    my ($b,$bs,$decrypt) = @_;
    return unless length $b;
    $b = length $b ? $b : '';
    if ($decrypt eq 'd') {
        $b=~ s/\0*$//s;
        return $b;
    }
    return $b . pack("C*", (0) x ($bs - length($b) % $bs));
}
