package Math::Round;

use strict;
use POSIX ();
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);

require Exporter;

@ISA = qw(Exporter AutoLoader);
@EXPORT = qw(round nearest);
@EXPORT_OK = qw(round nearest round_even round_odd round_rand
   nearest_ceil nearest_floor nearest_rand
   nlowmult nhimult );
$VERSION = '0.07';

%EXPORT_TAGS = ( all => [ @EXPORT_OK ] );

#--- Default value for "one-half". This is the lowest value that
#--- gives acceptable results for test #6 in test.pl. See the pod
#--- for more information.

$Math::Round::half = 0.50000000000008;

sub round {
 my $x;
 my @res  = map {
  if ($_ >= 0) { POSIX::floor($_ + $Math::Round::half); }
     else { POSIX::ceil($_ - $Math::Round::half); }
 } @_;

 return (wantarray) ? @res : $res[0];
}

sub round_even {
 my @res  = map {
   my ($sign, $in, $fr) = _sepnum($_);
   if ($fr == 0.5) {
      $sign * (($in % 2 == 0) ? $in : $in + 1);
   } else {
      $sign * POSIX::floor(abs($_) + $Math::Round::half);
   }
 } @_;
 return (wantarray) ? @res : $res[0];
}

sub round_odd {
 my @res  = map {
   my ($sign, $in, $fr) = _sepnum($_);
   if ($fr == 0.5) {
      $sign * (($in % 2 == 1) ? $in : $in + 1);
   } else {
      $sign * POSIX::floor(abs($_) + $Math::Round::half);
   }
 } @_;
 return (wantarray) ? @res : $res[0];
}

sub round_rand {
 my @res  = map {
   my ($sign, $in, $fr) = _sepnum($_);
   if ($fr == 0.5) {
      $sign * ((rand(4096) < 2048) ? $in : $in + 1);
   } else {
      $sign * POSIX::floor(abs($_) + $Math::Round::half);
   }
 } @_;
 return (wantarray) ? @res : $res[0];
}

#--- Separate a number into sign, integer, and fractional parts.
#--- Return as a list.
sub _sepnum {
 my $x = shift;
 my $sign = ($x >= 0) ? 1 : -1;
 $x = abs($x);
 my $i = int($x);
 return ($sign, $i, $x - $i);
}

#------ "Nearest" routines (round to a multiple of any number)

sub nearest {
 my $targ = abs(shift);
 my @res  = map {
  if ($_ >= 0) { $targ * int(($_ + $Math::Round::half * $targ) / $targ); }
     else { $targ * POSIX::ceil(($_ - $Math::Round::half * $targ) / $targ); }
 } @_;

 return (wantarray) ? @res : $res[0];
}

# In the next two functions, the code for positive and negative numbers
# turns out to be the same.  For negative numbers, the technique is not
# exactly obvious; instead of floor(x+0.5), we are in effect taking
# ceiling(x-0.5).

sub nearest_ceil {
    my $targ = abs(shift);
    my @res  = map { $targ * POSIX::floor(($_ + $Math::Round::half * $targ) / $targ) } @_;

    return wantarray ? @res : $res[0];
}

sub nearest_floor {
    my $targ = abs(shift);
    my @res  = map { $targ * POSIX::ceil(($_ - $Math::Round::half * $targ) / $targ) } @_;

    return wantarray ? @res : $res[0];
}

sub nearest_rand {
 my $targ = abs(shift);

 my @res = map {
   my ($sign, $in, $fr) = _sepnear($_, $targ);
   if ($fr == 0.5 * $targ) {
      $sign * $targ * ((rand(4096) < 2048) ? $in : $in + 1);
   } else {
      $sign * $targ * int((abs($_) + $Math::Round::half * $targ) / $targ);
   }
 } @_;
 return (wantarray) ? @res : $res[0];
}

#--- Next lower multiple
sub nlowmult {
    my $targ = abs(shift);
    my @res  = map { $targ * POSIX::floor($_ / $targ) } @_;

    return wantarray ? @res : $res[0];
}

#--- Next higher multiple
sub nhimult {
    my $targ = abs(shift);
    my @res  = map { $targ * POSIX::ceil($_ / $targ) } @_;

    return wantarray ? @res : $res[0];
}

#--- Separate a number into sign, "integer", and "fractional" parts
#--- for the 'nearest' calculation.  Return as a list.
sub _sepnear {
 my ($x, $targ) = @_;
 my $sign = ($x >= 0) ? 1 : -1;
 $x = abs($x);
 my $i = int($x / $targ);
 return ($sign, $i, $x - $i*$targ);
}

1;

__END__

=head1 NAME

Math::Round - Perl extension for rounding numbers

=head1 SYNOPSIS

  use Math::Round qw(...those desired... or :all);

  $rounded = round($scalar);
  @rounded = round(LIST...);
  $rounded = nearest($target, $scalar);
  @rounded = nearest($target, LIST...);

  # and other functions as described below

=head1 DESCRIPTION

B<Math::Round> supplies functions that will round numbers in different
ways.  The functions B<round> and B<nearest> are exported by
default; others are available as described below.  "use ... qw(:all)"
exports all functions.

=head1 FUNCTIONS

=over 2

=item B<round> LIST

Rounds the number(s) to the nearest integer.  In scalar context,
returns a single value; in list context, returns a list of values.
Numbers that are halfway between two integers are rounded
"to infinity"; i.e., positive values are rounded up (e.g., 2.5
becomes 3) and negative values down (e.g., -2.5 becomes -3).

Starting in Perl 5.22, the POSIX module by default exports all functions,
including one named "round". If you use both POSIX and this module,
exercise due caution.

=item B<round_even> LIST

Rounds the number(s) to the nearest integer.  In scalar context,
returns a single value; in list context, returns a list of values.
Numbers that are halfway between two integers are rounded to the
nearest even number; e.g., 2.5 becomes 2, 3.5 becomes 4, and -2.5
becomes -2.

=item B<round_odd> LIST

Rounds the number(s) to the nearest integer.  In scalar context,
returns a single value; in list context, returns a list of values.
Numbers that are halfway between two integers are rounded to the
nearest odd number; e.g., 3.5 becomes 3, 4.5 becomes 5, and -3.5
becomes -3.

=item B<round_rand> LIST

Rounds the number(s) to the nearest integer.  In scalar context,
returns a single value; in list context, returns a list of values.
Numbers that are halfway between two integers are rounded up or
down in a random fashion.  For example, in a large number of trials,
2.5 will become 2 half the time and 3 half the time.

=item B<nearest> TARGET, LIST

Rounds the number(s) to the nearest multiple of the target value.
TARGET must be positive.
In scalar context, returns a single value; in list context, returns
a list of values.  Numbers that are halfway between two multiples
of the target will be rounded to infinity.  For example:

  nearest(10, 44)    yields  40
  nearest(10, 46)            50
  nearest(10, 45)            50
  nearest(25, 328)          325
  nearest(.1, 4.567)          4.6
  nearest(10, -45)          -50

=item B<nearest_ceil> TARGET, LIST

Rounds the number(s) to the nearest multiple of the target value.
TARGET must be positive.
In scalar context, returns a single value; in list context, returns
a list of values.  Numbers that are halfway between two multiples
of the target will be rounded to the ceiling, i.e. the next
algebraically higher multiple.  For example:

  nearest_ceil(10, 44)    yields  40
  nearest_ceil(10, 45)            50
  nearest_ceil(10, -45)          -40

=item B<nearest_floor> TARGET, LIST

Rounds the number(s) to the nearest multiple of the target value.
TARGET must be positive.
In scalar context, returns a single value; in list context, returns
a list of values.  Numbers that are halfway between two multiples
of the target will be rounded to the floor, i.e. the next
algebraically lower multiple.  For example:

  nearest_floor(10, 44)    yields  40
  nearest_floor(10, 45)            40
  nearest_floor(10, -45)          -50

=item B<nearest_rand> TARGET, LIST

Rounds the number(s) to the nearest multiple of the target value.
TARGET must be positive.
In scalar context, returns a single value; in list context, returns
a list of values.  Numbers that are halfway between two multiples
of the target will be rounded up or down in a random fashion.
For example, in a large number of trials, C<nearest(10, 45)> will
yield 40 half the time and 50 half the time.

=item B<nlowmult> TARGET, LIST

Returns the next lower multiple of the number(s) in LIST.
TARGET must be positive.
In scalar context, returns a single value; in list context, returns
a list of values.  Numbers that are between two multiples of the
target will be adjusted to the nearest multiples of LIST that are
algebraically lower. For example:

  nlowmult(10, 44)    yields  40
  nlowmult(10, 46)            40
  nlowmult(25, 328)          325
  nlowmult(.1, 4.567)          4.5
  nlowmult(10, -41)          -50

=item B<nhimult> TARGET, LIST

Returns the next higher multiple of the number(s) in LIST.
TARGET must be positive.
In scalar context, returns a single value; in list context, returns
a list of values.  Numbers that are between two multiples of the
target will be adjusted to the nearest multiples of LIST that are
algebraically higher. For example:

  nhimult(10, 44)    yields  50
  nhimult(10, 46)            50
  nhimult(25, 328)          350
  nhimult(.1, 4.512)          4.6
  nhimult(10, -49)          -40

=back

=head1 VARIABLE

The variable B<$Math::Round::half> is used by most routines in this
module. Its value is very slightly larger than 0.5, for reasons
explained below. If you find that your application does not deliver
the expected results, you may reset this variable at will.

=head1 STANDARD FLOATING-POINT DISCLAIMER

Floating-point numbers are, of course, a rational subset of the real
numbers, so calculations with them are not always exact.
Numbers that are supposed to be halfway between
two others may surprise you; for instance, 0.85 may not be exactly
halfway between 0.8 and 0.9, and (0.75 - 0.7) may not be the same as
(0.85 - 0.8).

In order to give more predictable results, 
these routines use a value for
one-half that is slightly larger than 0.5.  Nevertheless,
if the numbers to be rounded are stored as floating-point, they will
be subject as usual to the mercies of your hardware, your C
compiler, etc.

=head1 AUTHOR

Math::Round was written by Geoffrey Rommel E<lt>GROMMEL@cpan.orgE<gt>
in October 2000.

=cut
