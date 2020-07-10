# ABSTRACT: Report errors from perspective of caller of a "clan" of modules

##
## Based on Carp.pm from Perl 5.005_03.
## Last modified 22-May-2016 by Kent Fredric.
## Should be reasonably backwards compatible.
##
## This module is free software and can
## be used, modified and redistributed
## under the same terms as Perl itself.
##

@DB::args = ();    # Avoid warning "used only once" in Perl 5.003

package Carp::Clan; # git description: v6.06-8-g0cfdd10

use strict;
use overload ();

# Original comments by Andy Wardley <abw@kfs.org> 09-Apr-1998.

# The $Max(EvalLen|(Arg(Len|Nums)) variables are used to specify how
# the eval text and function arguments should be formatted when printed.

our $MaxEvalLen = 0;     # How much eval '...text...' to show. 0 = all.
our $MaxArgLen  = 64;    # How much of each argument to print. 0 = all.
our $MaxArgNums = 8;     # How many arguments to print.        0 = all.

our $Verbose = 0;        # If true then make _shortmsg call _longmsg instead.

our $VERSION = '6.07';

# _longmsg() crawls all the way up the stack reporting on all the function
# calls made. The error string, $error, is originally constructed from the
# arguments passed into _longmsg() via confess(), cluck() or _shortmsg().
# This gets appended with the stack trace messages which are generated for
# each function call on the stack.

sub _longmsg {
    return (@_) if ( ref $_[0] );
    local $_;        # Protect surrounding program - just in case...
    my ( $pack, $file, $line, $sub, $hargs, $eval, $require, @parms, $push );
    my $error = join( '', @_ );
    my $msg   = '';
    my $i     = 0;
    while (
        do {
            {

                package # hide from PAUSE
                    DB;
                ( $pack, $file, $line, $sub, $hargs, undef, $eval, $require )
                    = caller( $i++ )
            }
        }
        )
    {
        next if ( $pack eq 'Carp::Clan' );
        if ( $error eq '' ) {
            if ( defined $eval ) {
                $eval =~ s/([\\\'])/\\$1/g unless ($require); # Escape \ and '
                $eval
                    =~ s/([\x00-\x1F\x7F-\xFF])/sprintf("\\x%02X",ord($1))/eg;
                substr( $eval, $MaxEvalLen ) = '...'
                    if ( $MaxEvalLen && length($eval) > $MaxEvalLen );
                if   ($require) { $sub = "require $eval"; }
                else            { $sub = "eval '$eval'"; }
            }
            elsif ( $sub eq '(eval)' ) { $sub = 'eval {...}'; }
            else {
                @parms = ();
                if ($hargs) {
                    $push  = 0;
                    @parms = @DB::args
                        ;    # We may trash some of the args so we take a copy
                    if ( $MaxArgNums and @parms > $MaxArgNums ) {
                        $#parms = $MaxArgNums;
                        pop(@parms);
                        $push = 1;
                    }
                    for (@parms) {
                        if ( defined $_ ) {
                            if ( ref $_ ) {
                                $_ = overload::StrVal($_);
                            }
                            else {
                                unless ( /^-?\d+(?:\.\d+(?:[eE][+-]\d+)?)?$/
                                    )    # Looks numeric
                                {
                                    s/([\\\'])/\\$1/g;    # Escape \ and '
                                    s/([\x00-\x1F\x7F-\xFF])/sprintf("\\x%02X",ord($1))/eg;
                                    substr( $_, $MaxArgLen ) = '...'
                                        if ( $MaxArgLen
                                        and length($_) > $MaxArgLen );
                                    $_ = "'$_'";
                                }
                            }
                        }
                        else { $_ = 'undef'; }
                    }
                    push( @parms, '...' ) if ($push);
                }
                $sub .= '(' . join( ', ', @parms ) . ')';
            }
            if ( $msg eq '' ) { $msg = "$sub called"; }
            else              { $msg .= "\t$sub called"; }
        }
        else {
            $msg = quotemeta($sub);
            if ( $error =~ /\b$msg\b/ ) { $msg = $error; }
            else {
                if ( $sub =~ /::/ ) { $msg = "$sub(): $error"; }
                else                { $msg = "$sub: $error"; }
            }
        }
        $msg .= " at $file line $line\n" unless ( $error =~ /\n$/ );
        $error = '';
    }
    $msg ||= $error;
    $msg =~ tr/\0//d;  # Circumvent die's incorrect handling of NUL characters
    $msg;
}

# _shortmsg() is called by carp() and croak() to skip all the way up to
# the top-level caller's package and report the error from there. confess()
# and cluck() generate a full stack trace so they call _longmsg() to
# generate that. In verbose mode _shortmsg() calls _longmsg() so you
# always get a stack trace.

sub _shortmsg {
    my $pattern = shift;
    my $verbose = shift;
    return (@_) if ( ref $_[0] );
    goto &_longmsg if ( $Verbose or $verbose );
    my ( $pack, $file, $line, $sub );
    my $error = join( '', @_ );
    my $msg   = '';
    my $i     = 0;
    while ( ( $pack, $file, $line, $sub ) = caller( $i++ ) ) {
        next if ( $pack eq 'Carp::Clan' or $pack =~ /$pattern/ );
        if ( $error eq '' ) { $msg = "$sub() called"; }
        else {
            $msg = quotemeta($sub);
            if ( $error =~ /\b$msg\b/ ) { $msg = $error; }
            else {
                if ( $sub =~ /::/ ) { $msg = "$sub(): $error"; }
                else                { $msg = "$sub: $error"; }
            }
        }
        $msg .= " at $file line $line\n" unless ( $error =~ /\n$/ );
        $msg =~ tr/\0//d; # Circumvent die's incorrect handling of NUL characters
        return $msg;
    }
    goto &_longmsg;
}

# In the two identical regular expressions (immediately after the two occurrences of
# "quotemeta") above, the "\b ... \b" helps to avoid confusion between function names
# which are prefixes of each other, e.g. "My::Class::print" and "My::Class::println".

# The following four functions call _longmsg() or _shortmsg() depending on
# whether they should generate a full stack trace (confess() and cluck())
# or simply report the caller's package (croak() and carp()), respectively.
# confess() and croak() die, carp() and cluck() warn.

# Following code kept for calls with fully qualified subroutine names:
# (For backward compatibility with the original Carp.pm)

sub croak {
    my $callpkg = caller(0);
    my $pattern = ( $callpkg eq 'main' ) ? '^:::' : "^$callpkg\$";
    die _shortmsg( $pattern, 0, @_ );
}
sub confess { die _longmsg(@_); }

sub carp {
    my $callpkg = caller(0);
    my $pattern = ( $callpkg eq 'main' ) ? '^:::' : "^$callpkg\$";
    warn _shortmsg( $pattern, 0, @_ );
}
sub cluck { warn _longmsg(@_); }

# The following method imports a different closure for every caller.
# I.e., different modules can use this module at the same time
# and in parallel and still use different patterns.

sub import {
    my $pkg     = shift;
    my $callpkg = caller(0);
    my $pattern = ( $callpkg eq 'main' ) ? '^:::' : "^$callpkg\$";
    my $verbose = 0;
    my $item;
    my $file;

    for $item (@_) {
        if ( $item =~ /^\d/ ) {
            if ( $VERSION < $item ) {
                $file = "$pkg.pm";
                $file =~ s!::!/!g;
                $file = $INC{$file};
                die _shortmsg( '^:::', 0,
                    "$pkg $item required--this is only version $VERSION ($file)"
                );
            }
        }
        elsif ( $item =~ /^verbose$/i ) { $verbose = 1; }
        else                            { $pattern = $item; }
    }

   # Speed up pattern matching in Perl versions >= 5.005:
   # (Uses "eval ''" because qr// is a syntax error in previous Perl versions)
    if ( $] >= 5.005 ) {
        eval '$pattern = qr/$pattern/;';
    }
    else {
        eval { $pkg =~ /$pattern/; };
    }
    if ($@) {
        $@ =~ s/\s+$//;
        $@ =~ s/\s+at\s.+$//;
        die _shortmsg( '^:::', 0, $@ );
    }
    {
        local ($^W) = 0;
        no strict "refs";
        *{"${callpkg}::croak"}   = sub { die  _shortmsg( $pattern, $verbose, @_ ); };
        *{"${callpkg}::confess"} = sub { die  _longmsg (                     @_ ); };
        *{"${callpkg}::carp"}    = sub { warn _shortmsg( $pattern, $verbose, @_ ); };
        *{"${callpkg}::cluck"}   = sub { warn _longmsg (                     @_ ); };
    }
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Carp::Clan - Report errors from perspective of caller of a "clan" of modules

=head1 VERSION

version 6.07

=head1 SYNOPSIS

 carp    - warn of errors (from perspective of caller)

 cluck   - warn of errors with stack backtrace

 croak   - die of errors (from perspective of caller)

 confess - die of errors with stack backtrace

    use Carp::Clan qw(^MyClan::);
    croak "We're outta here!";

    use Carp::Clan;
    confess "This is how we got here!";

=head1 DESCRIPTION

This module is based on "C<Carp.pm>" from Perl 5.005_03. It has been
modified to skip all package names matching the pattern given in
the "use" statement inside the "C<qw()>" term (or argument list).

Suppose you have a family of modules or classes named "Pack::A",
"Pack::B" and so on, and each of them uses "C<Carp::Clan qw(^Pack::);>"
(or at least the one in which the error or warning gets raised).

Thus when for example your script "tool.pl" calls module "Pack::A",
and module "Pack::A" calls module "Pack::B", an exception raised in
module "Pack::B" will appear to have originated in "tool.pl" where
"Pack::A" was called, and not in "Pack::A" where "Pack::B" was called,
as the unmodified "C<Carp.pm>" would try to make you believe C<:-)>.

This works similarly if "Pack::B" calls "Pack::C" where the
exception is raised, et cetera.

In other words, this blames all errors in the "C<Pack::*>" modules
on the user of these modules, i.e., on you. C<;-)>

The skipping of a clan (or family) of packages according to a pattern
describing its members is necessary in cases where these modules are
not classes derived from each other (and thus when examining C<@ISA>
- as in the original "C<Carp.pm>" module - doesn't help).

The purpose and advantage of this is that a "clan" of modules can work
together (and call each other) and throw exceptions at various depths
down the calling hierarchy and still appear as a monolithic block (as
though they were a single module) from the perspective of the caller.

In case you just want to ward off all error messages from the module
in which you "C<use Carp::Clan>", i.e., if you want to make all error
messages or warnings to appear to originate from where your module
was called (this is what you usually used to "C<use Carp;>" for C<;-)>),
instead of in your module itself (which is what you can do with a
"die" or "warn" anyway), you do not need to provide a pattern,
the module will automatically provide the correct one for you.

I.e., just "C<use Carp::Clan;>" without any arguments and call "carp"
or "croak" as appropriate, and they will automatically defend your
module against all blames!

In other words, a pattern is only necessary if you want to make
several modules (more than one) work together and appear as though
they were only one.

=head2 Forcing a Stack Trace

As a debugging aid, you can force "C<Carp::Clan>" to treat a "croak" as
a "confess" and a "carp" as a "cluck". In other words, force a detailed
stack trace to be given. This can be very helpful when trying to
understand why, or from where, a warning or error is being generated.

This feature is enabled either by "importing" the non-existent symbol
'verbose', or by setting the global variable "C<$Carp::Clan::Verbose>"
to a true value.

You would typically enable it by saying

    use Carp::Clan qw(verbose);

Note that you can both specify a "family pattern" and the string "verbose"
inside the "C<qw()>" term (or argument list) of the "use" statement, but
consider that a pattern of packages to skip is pointless when "verbose"
causes a full stack trace anyway.

=head1 BUGS

The "C<Carp::Clan>" routines don't handle exception objects currently.
If called with a first argument that is a reference, they simply
call "C<die()>" or "C<warn()>", as appropriate.

Bugs may be submitted through L<the RT bug tracker|https://rt.cpan.org/Public/Dist/Display.html?Name=Carp-Clan>
(or L<bug-Carp-Clan@rt.cpan.org|mailto:bug-Carp-Clan@rt.cpan.org>).

=head1 AUTHOR

Steffen Beyer <STBEY@cpan.org>

=head1 CONTRIBUTORS

=for stopwords Joshua ben Jore Karen Etheridge Kent Fredric

=over 4

=item *

Joshua ben Jore <jjore@cpan.org>

=item *

Karen Etheridge <ether@cpan.org>

=item *

Kent Fredric <kentnl@cpan.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2001 by Steffen Beyer, Joshua ben Jore.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
