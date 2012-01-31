package Embed::Persistent;

# p1.pl for Mod-Gearman

use strict;
use Text::ParseWords qw(parse_line);

use constant LEAVE_MSG   => 1;
use constant CACHE_DUMP  => 2;
use constant PLUGIN_DUMP => 4;
use constant DEBUG_LEVEL => 0;
# use constant DEBUG_LEVEL => CACHE_DUMP ;
# use constant DEBUG_LEVEL => LEAVE_MSG ;
# use constant DEBUG_LEVEL => LEAVE_MSG | CACHE_DUMP ;
# use constant DEBUG_LEVEL => LEAVE_MSG | CACHE_DUMP | PLUGIN_DUMP ;

use constant DEBUG_LOG_PATH         => './';
use constant LEAVE_MSG_STREAM       => DEBUG_LOG_PATH . 'epn_leave-msgs.log';
use constant CACHE_DUMP_STREAM      => DEBUG_LOG_PATH . 'epn_cache-dump.log';
use constant PLUGIN_DUMP_STREAM     => DEBUG_LOG_PATH . 'epn_plugin-dump.log';
use constant NUMBER_OF_PERL_PLUGINS => 60;

# Cache will be dumped every Cache_Dump_Interval plugin compilations
use constant Cache_Dump_Interval    => 20;

( DEBUG_LEVEL & LEAVE_MSG ) && do {
    open LH, '>> ' . LEAVE_MSG_STREAM
        or die "Can't open " . LEAVE_MSG_STREAM . ": $!";
    # Unbuffer LH since this will be written by child processes.
    select( ( select(LH), $| = 1 )[0] );
};
( DEBUG_LEVEL & CACHE_DUMP ) && do {
    ( open CH, '>> ' . CACHE_DUMP_STREAM or die "Can't open " . CACHE_DUMP_STREAM . ": $!" );
    select( ( select(CH), $| = 1 )[0] );
};
( DEBUG_LEVEL & PLUGIN_DUMP )
    && ( open PH, '>> ' . PLUGIN_DUMP_STREAM or die "Can't open " . PLUGIN_DUMP_STREAM . ": $!" );

require Data::Dumper
    if DEBUG_LEVEL & CACHE_DUMP;

my( %Cache, $Current_Run );
keys %Cache = NUMBER_OF_PERL_PLUGINS;

# Offsets in %Cache{$filename}
use constant MTIME        => 0;
use constant PLUGIN_ARGS  => 1;
use constant PLUGIN_ERROR => 2;
use constant PLUGIN_HNDLR => 3;

package main;

use subs 'CORE::GLOBAL::exit';

sub CORE::GLOBAL::exit { die "ExitTrap: $_[0] (Redefine exit to trap plugin exit with eval BLOCK)" }

package OutputTrap;

# Methods for use by tied STDOUT in embedded PERL module.
# Simply ties STDOUT to a scalar and caches values written to it.
# NB No more than 4KB characters per line are kept.

sub TIEHANDLE {
    my($class) = @_;
    my $me = '';
    bless \$me, $class;
}

sub PRINT {
    my $self = shift;

    $$self .= substr( join( '', @_ ), 0, 4096 );
}

sub PRINTF {
    my $self = shift;
    my $fmt  = shift;

    $$self .= substr( sprintf( $fmt, @_ ), 0, 4096 );
}

sub READLINE {
    my $self = shift;
    return $$self;
}

sub CLOSE {
    my $self = shift;
    undef $self;
}

sub DESTROY {
    my $self = shift;
    undef $self;
}

package Embed::Persistent;

sub valid_package_name {
    local $_ = shift;
    s|([^A-Za-z0-9\/])|sprintf("_%2x",unpack("C",$1))|eg;

    # second pass only for words starting with a digit
    s|/(\d)|sprintf("/_%2x",unpack("C",$1))|eg;

    # Dress it up as a real package name
    s|/|::|g;
    return /^::/ ? "Embed$_" : "Embed::$_";
}

# Perl 5.005_03 only traps warnings for errors classed by perldiag
# as Fatal (eg 'Global symbol """"%s"""" requires explicit package name').
# Therefore treat all warnings as fatal.

sub throw_exception {
    die shift;
}

sub eval_file {
    my( $filename, $delete, undef, $plugin_args ) = @_;

    my $mtime = -M $filename;
    my $ts    = localtime( time() )
        if DEBUG_LEVEL;

    if(    exists( $Cache{$filename} )
        && $Cache{$filename}[MTIME]
        && $Cache{$filename}[MTIME] <= $mtime )
    {

        # We have compiled this plugin before and
        # it has not changed on disk; nothing to do except
        # 1 parse the plugin arguments and cache them (to save
        #   repeated parsing of the same args) - the same
        #   plugin could be called with different args.
        # 2 return the error from a former compilation
        #   if there was one.

        $Cache{$filename}[PLUGIN_ARGS]{$plugin_args} ||= [ parse_line( '\s+', 0, $plugin_args ) ]
            if $plugin_args;

        if( $Cache{$filename}[PLUGIN_ERROR] ) {
            print LH qq($ts eval_file: $filename failed compilation formerly and file has not changed; skipping compilation.\n)
                if DEBUG_LEVEL & LEAVE_MSG;
            die qq(**ePN failed to compile $filename: "$Cache{$filename}[PLUGIN_ERROR]");
        }
        else {
            print LH qq($ts eval_file: $filename already successfully compiled and file has not changed; skipping compilation.\n)
                if DEBUG_LEVEL & LEAVE_MSG;
            return $Cache{$filename}[PLUGIN_HNDLR];
        }
    }

    my $package = valid_package_name($filename);

    $Cache{$filename}[PLUGIN_ARGS]{$plugin_args} ||= [ parse_line( '\s+', 0, $plugin_args ) ]
        if $plugin_args;

    local *FH;

    # die will be trapped by caller (checking ERRSV)
    open FH, $filename
        or die qq(**ePN failed to open "$filename": "$!");

    my $sub;
    sysread FH, $sub, -s FH;
    close FH;

    # Cater for scripts that have embedded EOF symbols (__END__)
    # XXXX
    # Doesn't make sense to me.

    # $sub		=~ s/__END__/\;}\n__END__/;
    # Wrap the code into a subroutine inside our unique package
    # (using $_ here [to save a lexical] is not a good idea since
    # the definition of the package is visible to any other Perl
    # code that uses [non localised] $_).
    my $hndlr = <<EOSUB ;
package $package;

sub hndlr {
    \@ARGV = \@_;
    local \$^W = 1;
    \$ENV{NAGIOS_PLUGIN} = '$filename';

# <<< START of PLUGIN (first line of plugin is line 8 in the text) >>>
$sub
# <<< END of PLUGIN >>>
}
EOSUB

    $Cache{$filename}[MTIME] = $mtime
        unless $delete;

    # Suppress warning display.
    local $SIG{__WARN__} = \&throw_exception;

    # Following 3 lines added 10/18/07 by Larry Low to fix problem where
    # modified Perl plugins didn't get recached by the epn
    no strict 'refs';
    undef %{ $package . '::' };
    use strict 'refs';

    # Compile &$package::hndlr. Since non executable code is being eval'd
    # there is no need to protect lexicals in this scope.
    eval $hndlr;

    # $@ is set for any warning and error.
    # This guarantees that the plugin will not be run.
    if($@) {

        # Report error line number wrt to original plugin text (7 lines added by eval_file).
        # Error text looks like
        # 'Use of uninitialized ..' at (eval 23) line 186, <DATA> line 218
        # The error line number is 'line 186'
        chomp($@);
        $@ =~ s/line (\d+)[\.,]/'line ' . ($1 - 7) . ','/e;

        print LH qq($ts eval_file: syntax error in $filename: "$@".\n)
            if DEBUG_LEVEL & LEAVE_MSG;

        if( DEBUG_LEVEL & PLUGIN_DUMP ) {
            my $i = 1;
            $_ = $hndlr;
            s/^/sprintf('%10d  ', $i++)/meg;

            # Will only get here once (when a faulty plugin is compiled).
            # Therefore only _faulty_ plugins are dumped once each time the text changes.

            print PH qq($ts eval_file: transformed plugin "$filename" to ==>\n$_\n);
        }

        $@ = substr( $@, 0, 4096 )
            if length($@) > 4096;

        $Cache{$filename}[PLUGIN_ERROR] = $@;

        # If the compilation fails, leave nothing behind that may affect subsequent
        # compilations. This will be trapped by caller (checking ERRSV).
        die qq(**ePN failed to compile $filename: "$@");

    }
    else {
        $Cache{$filename}[PLUGIN_ERROR] = '';
    }

    print LH qq($ts eval_file: successfully compiled "$filename $plugin_args".\n)
        if DEBUG_LEVEL & LEAVE_MSG;

    print CH qq($ts eval_file: after $Current_Run compilations \%Cache =>\n), Data::Dumper->Dump( [ \%Cache ], [qw(*Cache)] ), "\n"
        if( ( DEBUG_LEVEL & CACHE_DUMP ) && ( ++$Current_Run % Cache_Dump_Interval == 0 ) );

    no strict 'refs';
    return $Cache{$filename}[PLUGIN_HNDLR] = *{ $package . '::hndlr' }{CODE};

}

sub run_package {
    my( $filename, undef, $plugin_hndlr_cr, $plugin_args ) = @_;

    # Second parm (after $filename) _may_ be used to wallop stashes.

    my $res = 3;
    my $ts  = localtime( time() )
        if DEBUG_LEVEL;

    local $SIG{__WARN__} = \&throw_exception;
    my $stdout = tie( *STDOUT, 'OutputTrap' );
    my @plugin_args = $plugin_args ? @{ $Cache{$filename}[PLUGIN_ARGS]{$plugin_args} } : ();

    # If the plugin has args, they have been cached by eval_file.
    # ( cannot cache @plugin_args here because run_package() is
    #   called by child processes so cannot update %Cache.)

    eval { $plugin_hndlr_cr->(@plugin_args) };

    if($@) {

        # Error => normal plugin termination (exit) || run time error.
        $_ = $@;
        /^ExitTrap: (-?\d+)/ ? $res = $1 :

            # For normal plugin exit, $@ will  always match /^ExitTrap: (-?\d+)/
            /^ExitTrap:  / ? $res = 0 : do {

            # Run time error/abnormal plugin termination.

            chomp;

            # Report error line number wrt to original plugin text (7 lines added by eval_file).
            s/line (\d+)[\.,]/'line ' . ($1 - 7) . ','/e;
            print STDOUT qq(**ePN $filename: "$_".\n);
            };

        ( $@, $_ ) = ( '', '' );
    }

    # ! Error => Perl code is not a plugin (fell off the end; no exit)

    # !! (read any output from the tied file handle.)
    my $plugin_output = <STDOUT>;

    undef $stdout;
    untie *STDOUT;

    print LH qq($ts run_package: "$filename $plugin_args" returning ($res, "$plugin_output").\n)
        if DEBUG_LEVEL & LEAVE_MSG;

    return ( $res, $plugin_output );
}

1;

=head1 SEE ALSO

=over 4

=item * perlembed (section on maintaining a persistent interpreter)

=back

=head1 AUTHOR

Originally by Stephen Davies.

Now maintained by Stanley Hopcroft <hopcrofts@cpan.org> who retains responsibility for the 'bad bits'.

=head1 COPYRIGHT

Copyright (c) 2004 Stanley Hopcroft. All rights reserved.
This program is free software; you can redistribute it and/or modify it under the same terms as Perl itself.

=cut
