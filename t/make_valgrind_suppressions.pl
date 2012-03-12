#!/usr/bin/perl
use strict;
use warnings;

die("please run from project root") unless -d 't';
create_suppressions();
split_suppressions();
exit 0;

#################################################
sub create_suppressions {
    `make clean >/dev/null && ./configure --enable-embedded-perl --enable-debug && make >/dev/null`;
    `>t/valgrind_suppress.cfg`;

    my @tests = split/\s+/, `grep ^check_PROGRAMS Makefile.am | awk -F = '{print \$2}'`;
    for my $test (@tests) {
        next if $test =~ m/^\s*$/;
        print "$test...\n";
        `make $test >/dev/null 2>&1 && yes | valgrind --tool=memcheck --leak-check=yes --leak-check=full --show-reachable=yes --track-origins=yes --gen-suppressions=yes ./$test >> t/valgrind_suppress.cfg 2>&1`;
    }
}

#################################################
sub split_suppressions {
    my $x = 1;
    my $all_suppressions = {};
    if(-f '/tmp/suppressions.log') {
        print `cat /tmp/suppressions.log >> t/valgrind_suppress.cfg`;
    }
    print `ls -la t/valgrind_suppress.cfg`;
    open(my $fh, '<', 't/valgrind_suppress.cfg') or die($!);
    my $text;
    while(my $line = <$fh>) {
        next if $line =~ m/^\s*$/;
        next if $line =~ m/^{$/;
        next if $line =~ m/^#/;
        next if $line =~ m/^==/;
        next if $line =~ m/^ok/;
        next if $line =~ m/^not\s+ok/;
        next if $line =~ m/^1\.\.\d+$/;
        next if $line =~ m/^\[\d+\-\d+\-\d+\s+/;
        next if $line =~ m/^core logger is not available/;

        if($line =~ m/^\s+<insert_a_suppression_name_here/) {
            $text = "{\n";
        }
        die("line: ".$line) unless defined $text;
        $text .= $line;
        if($line =~ m/^\s*}\s*$/) {
            $all_suppressions->{$text} = 1 if $text =~ m/Perl_/;
            undef $text;
        }
    }
    die("unmatched entry") if defined $text;
    close($fh);

    my @sorted = sort keys %{$all_suppressions};

    open($fh, '>', 't/valgrind_suppress.cfg') or die("cannot open file: $!");
    print $fh join("\n", @sorted);
    close($fh);
    print `cat t/valgrind_extra_manual.cfg >> t/valgrind_suppress.cfg`;
    print `ls -la t/valgrind_suppress.cfg`;
    return;
}
