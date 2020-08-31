use strict;
use warnings;

package Test::Number::Delta;
# ABSTRACT: Compare the difference between numbers against a given tolerance

our $VERSION = '1.06';

use vars qw (@EXPORT @ISA);

# Required modules
use Carp;
use Test::Builder;
use Exporter;

@ISA    = qw( Exporter );
@EXPORT = qw( delta_not_ok delta_ok delta_within delta_not_within );

#pod =head1 SYNOPSIS
#pod
#pod   # Import test functions
#pod   use Test::Number::Delta;
#pod
#pod   # Equality test with default tolerance
#pod   delta_ok( 1e-5, 2e-5, 'values within 1e-6');
#pod
#pod   # Inequality test with default tolerance
#pod   delta_not_ok( 1e-5, 2e-5, 'values not within 1e-6');
#pod
#pod   # Provide specific tolerance
#pod   delta_within( 1e-3, 2e-3, 1e-4, 'values within 1e-4');
#pod   delta_not_within( 1e-3, 2e-3, 1e-4, 'values not within 1e-4');
#pod
#pod   # Compare arrays or matrices
#pod   @a = ( 3.14, 1.41 );
#pod   @b = ( 3.15, 1.41 );
#pod   delta_ok( \@a, \@b, 'compare @a and @b' );
#pod
#pod   # Set a different default tolerance
#pod   use Test::Number::Delta within => 1e-5;
#pod   delta_ok( 1.1e-5, 2e-5, 'values within 1e-5'); # ok
#pod
#pod   # Set a relative tolerance
#pod   use Test::Number::Delta relative => 1e-3;
#pod   delta_ok( 1.01, 1.0099, 'values within 1.01e-3');
#pod
#pod
#pod =head1 DESCRIPTION
#pod
#pod At some point or another, most programmers find they need to compare
#pod floating-point numbers for equality.  The typical idiom is to test
#pod if the absolute value of the difference of the numbers is within a desired
#pod tolerance, usually called epsilon.  This module provides such a function for use
#pod with L<Test::More>.  Usage is similar to other test functions described in
#pod L<Test::More>.  Semantically, the C<delta_within> function replaces this kind
#pod of construct:
#pod
#pod  ok ( abs($p - $q) < $epsilon, '$p is equal to $q' ) or
#pod      diag "$p is not equal to $q to within $epsilon";
#pod
#pod While there's nothing wrong with that construct, it's painful to type it
#pod repeatedly in a test script.  This module does the same thing with a single
#pod function call.  The C<delta_ok> function is similar, but either uses a global
#pod default value for epsilon or else calculates a 'relative' epsilon on
#pod the fly so that epsilon is scaled automatically to the size of the arguments to
#pod C<delta_ok>.  Both functions are exported automatically.
#pod
#pod Because checking floating-point equality is not always reliable, it is not
#pod possible to check the 'equal to' boundary of 'less than or equal to
#pod epsilon'.  Therefore, Test::Number::Delta only compares if the absolute value
#pod of the difference is B<less than> epsilon (for equality tests) or
#pod B<greater than> epsilon (for inequality tests).
#pod
#pod =head1 USAGE
#pod
#pod =head2 use Test::Number::Delta;
#pod
#pod With no arguments, epsilon defaults to 1e-6. (An arbitrary choice on the
#pod author's part.)
#pod
#pod =head2 use Test::Number::Delta within => 1e-9;
#pod
#pod To specify a different default value for epsilon, provide a C<within> parameter
#pod when importing the module.  The value must be non-zero.
#pod
#pod =head2 use Test::Number::Delta relative => 1e-3;
#pod
#pod As an alternative to using a fixed value for epsilon, provide a C<relative>
#pod parameter when importing the module.  This signals that C<delta_ok> should
#pod test equality with an epsilon that is scaled to the size of the arguments.
#pod Epsilon is calculated as the relative value times the absolute value
#pod of the argument with the greatest magnitude.  Mathematically, for arguments
#pod 'x' and 'y':
#pod
#pod  epsilon = relative * max( abs(x), abs(y) )
#pod
#pod For example, a relative value of "0.01" would mean that the arguments are equal
#pod if they differ by less than 1% of the larger of the two values.  A relative
#pod value of 1e-6 means that the arguments must differ by less than 1 millionth
#pod of the larger value.  The relative value must be non-zero.
#pod
#pod =head2 Combining with a test plan
#pod
#pod  use Test::Number::Delta 'no_plan';
#pod
#pod  # or
#pod
#pod  use Test::Number::Delta within => 1e-9, tests => 1;
#pod
#pod If a test plan has not already been specified, the optional
#pod parameter for Test::Number::Delta may be followed with a test plan (see
#pod L<Test::More> for details).  If a parameter for Test::Number::Delta is
#pod given, it must come first.
#pod
#pod =cut

my $Test     = Test::Builder->new;
my $Epsilon  = 1e-6;
my $Relative = undef;

sub import {
    my $self  = shift;
    my $pack  = caller;
    my $found = grep /within|relative/, @_;
    croak "Can't specify more than one of 'within' or 'relative'"
      if $found > 1;
    if ($found) {
        my ( $param, $value ) = splice @_, 0, 2;
        croak "'$param' parameter must be non-zero"
          if $value == 0;
        if ( $param eq 'within' ) {
            $Epsilon = abs($value);
        }
        elsif ( $param eq 'relative' ) {
            $Relative = abs($value);
        }
        else {
            croak "Test::Number::Delta parameters must come first";
        }
    }
    $Test->exported_to($pack);
    $Test->plan(@_);
    $self->export_to_level( 1, $self, $_ ) for @EXPORT;
}

#--------------------------------------------------------------------------#
# _check -- recursive function to perform comparison
#--------------------------------------------------------------------------#

sub _check {
    my ( $p, $q, $e, $name, @indices ) = @_;
    my $epsilon;

    if ( !defined $e ) {
        $epsilon =
            $Relative
          ? $Relative * ( abs($p) > abs($q) ? abs($p) : abs($q) )
          : $Epsilon;
    }
    else {
        $epsilon = abs($e);
    }

    my ( $ok, $diag ) = ( 1, q{} ); # assume true
    if ( ref $p eq 'ARRAY' || ref $q eq 'ARRAY' ) {
        if ( @$p == @$q ) {
            for my $i ( 0 .. $#{$p} ) {
                my @new_indices;
                ( $ok, $diag, @new_indices ) =
                  _check( $p->[$i], $q->[$i], $e, $name, scalar @indices ? @indices : (), $i, );
                if ( not $ok ) {
                    @indices = @new_indices;
                    last;
                }
            }
        }
        else {
            $ok = 0;
            $diag =
                "Got an array of length "
              . scalar(@$p)
              . ", but expected an array of length "
              . scalar(@$q);
        }
    }
    else {
        $ok = $p == $q || abs( $p - $q ) < $epsilon;
        if ( !$ok ) {
            my ( $ep, $dp ) = _ep_dp($epsilon);
            $diag = sprintf( "%.${dp}f and %.${dp}f are not equal" . " to within %.${ep}f",
                $p, $q, $epsilon );
        }
    }
    return ( $ok, $diag, scalar(@indices) ? @indices : () );
}

sub _ep_dp {
    my $epsilon = shift;
    return ( 0, 0 ) unless $epsilon;
    $epsilon = abs($epsilon);
    my ($exp) = sprintf( "%e", $epsilon ) =~ m/e(.+)/;
    my $ep = $exp < 0 ? -$exp : 1;
    my $dp = $ep + 1;
    return ( $ep, $dp );
}

sub _diag_default {
    my ($ep) = _ep_dp( abs( $Relative || $Epsilon ) );
    my $diag = "Arguments are equal to within ";
    $diag .=
      $Relative
      ? sprintf( "relative tolerance %.${ep}f", abs($Relative) )
      : sprintf( "%.${ep}f",                    abs($Epsilon) );
    return $diag;
}

#pod =head1 FUNCTIONS
#pod
#pod =cut

#--------------------------------------------------------------------------#
# delta_within()
#--------------------------------------------------------------------------#

#pod =head2 delta_within
#pod
#pod  delta_within(  $p,  $q, $epsilon, '$p and $q are equal within $epsilon' );
#pod  delta_within( \@p, \@q, $epsilon, '@p and @q are equal within $epsilon' );
#pod
#pod This function tests for equality within a given value of epsilon. The test is
#pod true if the absolute value of the difference between $p and $q is B<less than>
#pod epsilon.  If the test is true, it prints an "OK" statement for use in testing.
#pod If the test is not true, this function prints a failure report and diagnostic.
#pod Epsilon must be non-zero.
#pod
#pod The values to compare may be scalars or references to arrays.  If the values
#pod are references to arrays, the comparison is done pairwise for each index value
#pod of the array.  The pairwise comparison is recursive, so matrices may
#pod be compared as well.
#pod
#pod For example, this code sample compares two matrices:
#pod
#pod     my @a = (   [ 3.14, 6.28 ],
#pod                 [ 1.41, 2.84 ]   );
#pod
#pod     my @b = (   [ 3.14, 6.28 ],
#pod                 [ 1.42, 2.84 ]   );
#pod
#pod     delta_within( \@a, \@b, 1e-6, 'compare @a and @b' );
#pod
#pod The sample prints the following:
#pod
#pod     not ok 1 - compare @a and @b
#pod     # At [1][0]: 1.4100000 and 1.4200000 are not equal to within 0.000001
#pod
#pod =cut

sub delta_within($$$;$) { ## no critic
    my ( $p, $q, $epsilon, $name ) = @_;
    croak "Value of epsilon to delta_within must be non-zero"
      if !defined($epsilon) || $epsilon == 0;
    {
        local $Test::Builder::Level = $Test::Builder::Level + 1;
        _delta_within( $p, $q, $epsilon, $name );
    }
}

sub _delta_within {
    my ( $p, $q, $epsilon, $name ) = @_;
    my ( $ok, $diag, @indices ) = _check( $p, $q, $epsilon, $name );
    if (@indices) {
        $diag = "At [" . join( "][", @indices ) . "]: $diag";
    }
    return $Test->ok( $ok, $name ) || $Test->diag($diag);
}

#--------------------------------------------------------------------------#
# delta_ok()
#--------------------------------------------------------------------------#

#pod =head2 delta_ok
#pod
#pod  delta_ok(  $p,  $q, '$p and $q are close enough to equal' );
#pod  delta_ok( \@p, \@q, '@p and @q are close enough to equal' );
#pod
#pod This function tests for equality within a default epsilon value.  See L</USAGE>
#pod for details on changing the default.  Otherwise, this function works the same
#pod as C<delta_within>.
#pod
#pod =cut

sub delta_ok($$;$) { ## no critic
    my ( $p, $q, $name ) = @_;
    {
        local $Test::Builder::Level = $Test::Builder::Level + 1;
        _delta_within( $p, $q, undef, $name );
    }
}

#--------------------------------------------------------------------------#
# delta_not_ok()
#--------------------------------------------------------------------------#

#pod =head2 delta_not_within
#pod
#pod  delta_not_within(  $p,  $q, '$p and $q are different' );
#pod  delta_not_within( \@p, \@q, $epsilon, '@p and @q are different' );
#pod
#pod This test compares inequality in excess of a given value of epsilon. The test
#pod is true if the absolute value of the difference between $p and $q is B<greater
#pod than> epsilon.  For array or matrix comparisons, the test is true if I<any>
#pod pair of values differs by more than epsilon.  Otherwise, this function works
#pod the same as C<delta_within>.
#pod
#pod =cut

sub delta_not_within($$$;$) { ## no critic
    my ( $p, $q, $epsilon, $name ) = @_;
    croak "Value of epsilon to delta_not_within must be non-zero"
      if !defined($epsilon) || $epsilon == 0;
    {
        local $Test::Builder::Level = $Test::Builder::Level + 1;
        _delta_not_within( $p, $q, $epsilon, $name );
    }
}

sub _delta_not_within($$$;$) { ## no critic
    my ( $p, $q, $epsilon, $name ) = @_;
    my ( $ok, undef, @indices ) = _check( $p, $q, $epsilon, $name );
    $ok = !$ok;
    my ( $ep, $dp ) = _ep_dp($epsilon);
    my $diag =
      defined($epsilon)
      ? sprintf( "Arguments are equal to within %.${ep}f", abs($epsilon) )
      : _diag_default();
    return $Test->ok( $ok, $name ) || $Test->diag($diag);
}

#pod =head2 delta_not_ok
#pod
#pod  delta_not_ok(  $p,  $q, '$p and $q are different' );
#pod  delta_not_ok( \@p, \@q, '@p and @q are different' );
#pod
#pod This function tests for inequality in excess of a default epsilon value.  See
#pod L</USAGE> for details on changing the default.  Otherwise, this function works
#pod the same as C<delta_not_within>.
#pod
#pod =cut

sub delta_not_ok($$;$) { ## no critic
    my ( $p, $q, $name ) = @_;
    {
        local $Test::Builder::Level = $Test::Builder::Level + 1;
        _delta_not_within( $p, $q, undef, $name );
    }
}

#pod =head1 SEE ALSO
#pod
#pod =for :list
#pod * L<Number::Tolerant>
#pod * L<Test::Deep::NumberTolerant>
#pod
#pod =cut

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test::Number::Delta - Compare the difference between numbers against a given tolerance

=head1 VERSION

version 1.06

=head1 SYNOPSIS

  # Import test functions
  use Test::Number::Delta;

  # Equality test with default tolerance
  delta_ok( 1e-5, 2e-5, 'values within 1e-6');

  # Inequality test with default tolerance
  delta_not_ok( 1e-5, 2e-5, 'values not within 1e-6');

  # Provide specific tolerance
  delta_within( 1e-3, 2e-3, 1e-4, 'values within 1e-4');
  delta_not_within( 1e-3, 2e-3, 1e-4, 'values not within 1e-4');

  # Compare arrays or matrices
  @a = ( 3.14, 1.41 );
  @b = ( 3.15, 1.41 );
  delta_ok( \@a, \@b, 'compare @a and @b' );

  # Set a different default tolerance
  use Test::Number::Delta within => 1e-5;
  delta_ok( 1.1e-5, 2e-5, 'values within 1e-5'); # ok

  # Set a relative tolerance
  use Test::Number::Delta relative => 1e-3;
  delta_ok( 1.01, 1.0099, 'values within 1.01e-3');

=head1 DESCRIPTION

At some point or another, most programmers find they need to compare
floating-point numbers for equality.  The typical idiom is to test
if the absolute value of the difference of the numbers is within a desired
tolerance, usually called epsilon.  This module provides such a function for use
with L<Test::More>.  Usage is similar to other test functions described in
L<Test::More>.  Semantically, the C<delta_within> function replaces this kind
of construct:

 ok ( abs($p - $q) < $epsilon, '$p is equal to $q' ) or
     diag "$p is not equal to $q to within $epsilon";

While there's nothing wrong with that construct, it's painful to type it
repeatedly in a test script.  This module does the same thing with a single
function call.  The C<delta_ok> function is similar, but either uses a global
default value for epsilon or else calculates a 'relative' epsilon on
the fly so that epsilon is scaled automatically to the size of the arguments to
C<delta_ok>.  Both functions are exported automatically.

Because checking floating-point equality is not always reliable, it is not
possible to check the 'equal to' boundary of 'less than or equal to
epsilon'.  Therefore, Test::Number::Delta only compares if the absolute value
of the difference is B<less than> epsilon (for equality tests) or
B<greater than> epsilon (for inequality tests).

=head1 USAGE

=head2 use Test::Number::Delta;

With no arguments, epsilon defaults to 1e-6. (An arbitrary choice on the
author's part.)

=head2 use Test::Number::Delta within => 1e-9;

To specify a different default value for epsilon, provide a C<within> parameter
when importing the module.  The value must be non-zero.

=head2 use Test::Number::Delta relative => 1e-3;

As an alternative to using a fixed value for epsilon, provide a C<relative>
parameter when importing the module.  This signals that C<delta_ok> should
test equality with an epsilon that is scaled to the size of the arguments.
Epsilon is calculated as the relative value times the absolute value
of the argument with the greatest magnitude.  Mathematically, for arguments
'x' and 'y':

 epsilon = relative * max( abs(x), abs(y) )

For example, a relative value of "0.01" would mean that the arguments are equal
if they differ by less than 1% of the larger of the two values.  A relative
value of 1e-6 means that the arguments must differ by less than 1 millionth
of the larger value.  The relative value must be non-zero.

=head2 Combining with a test plan

 use Test::Number::Delta 'no_plan';

 # or

 use Test::Number::Delta within => 1e-9, tests => 1;

If a test plan has not already been specified, the optional
parameter for Test::Number::Delta may be followed with a test plan (see
L<Test::More> for details).  If a parameter for Test::Number::Delta is
given, it must come first.

=head1 FUNCTIONS

=head2 delta_within

 delta_within(  $p,  $q, $epsilon, '$p and $q are equal within $epsilon' );
 delta_within( \@p, \@q, $epsilon, '@p and @q are equal within $epsilon' );

This function tests for equality within a given value of epsilon. The test is
true if the absolute value of the difference between $p and $q is B<less than>
epsilon.  If the test is true, it prints an "OK" statement for use in testing.
If the test is not true, this function prints a failure report and diagnostic.
Epsilon must be non-zero.

The values to compare may be scalars or references to arrays.  If the values
are references to arrays, the comparison is done pairwise for each index value
of the array.  The pairwise comparison is recursive, so matrices may
be compared as well.

For example, this code sample compares two matrices:

    my @a = (   [ 3.14, 6.28 ],
                [ 1.41, 2.84 ]   );

    my @b = (   [ 3.14, 6.28 ],
                [ 1.42, 2.84 ]   );

    delta_within( \@a, \@b, 1e-6, 'compare @a and @b' );

The sample prints the following:

    not ok 1 - compare @a and @b
    # At [1][0]: 1.4100000 and 1.4200000 are not equal to within 0.000001

=head2 delta_ok

 delta_ok(  $p,  $q, '$p and $q are close enough to equal' );
 delta_ok( \@p, \@q, '@p and @q are close enough to equal' );

This function tests for equality within a default epsilon value.  See L</USAGE>
for details on changing the default.  Otherwise, this function works the same
as C<delta_within>.

=head2 delta_not_within

 delta_not_within(  $p,  $q, '$p and $q are different' );
 delta_not_within( \@p, \@q, $epsilon, '@p and @q are different' );

This test compares inequality in excess of a given value of epsilon. The test
is true if the absolute value of the difference between $p and $q is B<greater
than> epsilon.  For array or matrix comparisons, the test is true if I<any>
pair of values differs by more than epsilon.  Otherwise, this function works
the same as C<delta_within>.

=head2 delta_not_ok

 delta_not_ok(  $p,  $q, '$p and $q are different' );
 delta_not_ok( \@p, \@q, '@p and @q are different' );

This function tests for inequality in excess of a default epsilon value.  See
L</USAGE> for details on changing the default.  Otherwise, this function works
the same as C<delta_not_within>.

=head1 SEE ALSO

=over 4

=item *

L<Number::Tolerant>

=item *

L<Test::Deep::NumberTolerant>

=back

=for :stopwords cpan testmatrix url annocpan anno bugtracker rt cpants kwalitee diff irc mailto metadata placeholders metacpan

=head1 SUPPORT

=head2 Bugs / Feature Requests

Please report any bugs or feature requests through the issue tracker
at L<https://github.com/dagolden/Test-Number-Delta/issues>.
You will be notified automatically of any progress on your issue.

=head2 Source Code

This is open source software.  The code repository is available for
public review and contribution under the terms of the license.

L<https://github.com/dagolden/Test-Number-Delta>

  git clone https://github.com/dagolden/Test-Number-Delta.git

=head1 AUTHOR

David Golden <dagolden@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2014 by David Golden.

This is free software, licensed under:

  The Apache License, Version 2.0, January 2004

=cut
