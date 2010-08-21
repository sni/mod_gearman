#!/usr/bin/perl

use Data::Dumper;
use Carp;
use Net::Telnet::Gearman;
use Text::TabularDisplay;

my $session = Net::Telnet::Gearman->new(
    Host => '127.0.0.1',
    Port => 4000,
);


while(1) {
    system("clear");
    print scalar localtime;
    print "\n";

    #my @workers   = $session->workers();
    #print_data("Worker", \@workers);
    #print Dumper(\@workers);

    my @functions = $session->status();
    print_data("Functions", \@functions);
    #print Dumper(\@functions);

    sleep(2);
}

$session->shutdown('graceful');

sub print_data {
    my($name,$data) = @_;;
    print $name."\n";
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
