#!/usr/bin/env perl
use strict;
use warnings;

die("please run from project root") unless -d 't';
create_suppressions();
split_suppressions();
uniq_suppressions();
exit 0;

#################################################
sub create_suppressions {
    mkdir('t/out') or die("mkdir failed: $!");
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
    my $fh2;
    if(-f '/tmp/suppressions.log') {
        `cat /tmp/suppressions.log >> t/valgrind_suppress.cfg`;
    }
    print `ls -la t/valgrind_suppress.cfg`;
    open(my $fh, '<', 't/valgrind_suppress.cfg') or die($!);
    while(my $line = <$fh>) {
        next if $line =~ m/^#/;
        next if $line =~ m/^==/;
        next if $line =~ m/^ok/;
        next if $line =~ m/^not\s+ok/;
        next if $line =~ m/^1\.\.\d+$/;
        next if $line =~ m/^\[\d+\-\d+\-\d+\s+/;
        next if $line =~ m/^core logger is not available/;

        if($line =~ m/^\s+<insert_a_suppression_name_here/) {
            $fh2 = open_new_file($fh2, $x++);
            print $fh2 "{\n";
        }
        die("line: ".$line) unless defined $fh2;
        print $fh2 $line;
        if($line =~ m/^\s*}\s*$/) {
            close($fh2);
            undef $fh2;
        }
    }
    die("open file") if defined $fh2;
}

#################################################
sub uniq_suppressions {
    `fdupes -Nd t/out/`;
    `grep -ci perl t/out/* | grep :0 | awk -F : '{ print \$1 }' | xargs -r rm`;
    `cat t/out/* > t/valgrind_suppress.cfg`;
    `rm -rf t/out`;
    print "striped:\n";
    print `ls -la t/valgrind_suppress.cfg`;
}

#################################################
sub open_new_file {
    my $fh2 = shift;
    my $nr  = shift;
    close($fh2) if defined $fh2;
    my $file = sprintf('t/out/%08d.cfg', $nr);
    open $fh2, '>', $file or die $file.": ".$!;
    return $fh2;
}
