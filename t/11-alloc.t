#!/usr/bin/perl

use warnings;
use strict;
use Test::More tests => 38;
use Data::Dumper;

for my $file (sort split("\n", `find common/ include/ neb_module/ tools/ worker/ -type f`)) {
    next if $file !~ m/\.(c|h)$/mx;
    next if $file =~ m/gm_alloc\.(c|h)/mx;
    next if $file =~ m/base64\.c/mx;
    next if $file =~ m/include\/nagios/mx;
    my $content = read_file($file);
    $content =~ s|(/\*.*?\*/)|&_replace_comments($1)|gsmxe;
    $content =~ s|(//.*)$|&_replace_comments($1)|gmxe;
    my $errors  = 0;
    my $linenr  = 0;
    for my $line (split(/\n/mx, $content)) {
        $linenr++;
        if($line =~ m/(^|[^_]+)(asprintf|malloc|calloc|realloc|strdup|strndup)/mx) {
            $errors++;
            fail($file.':'.$linenr." ".$line);
        }
    }
    ok($errors == 0, $file." is ok");
}
exit(0);

# replace comments with space, so they don't match our pattern matches later
sub _replace_comments {
    my($comment) = @_;
    $comment =~ s/./ /gmx;
    return($comment);
}

sub read_file {
    my($filename) = @_;
    my $content = "";
    open(my $fh, '<', $filename) or die("cannot read $filename: $!");
    while(my $line = <$fh>) { $content .= $line; }
    close($fh);
    return($content);
}
