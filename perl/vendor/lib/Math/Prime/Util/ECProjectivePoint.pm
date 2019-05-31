package Math::Prime::Util::ECProjectivePoint;
use strict;
use warnings;
use Carp qw/carp croak confess/;

BEGIN {
  $Math::Prime::Util::ECProjectivePoint::AUTHORITY = 'cpan:DANAJ';
  $Math::Prime::Util::ECProjectivePoint::VERSION = '0.73';
}

BEGIN {
  do { require Math::BigInt;  Math::BigInt->import(try=>"GMP,Pari"); }
    unless defined $Math::BigInt::VERSION;
}

# Pure perl (with Math::BigInt) manipulation of Elliptic Curves
# in projective coordinates.

sub new {
  my ($class, $c, $n, $x, $z) = @_;
  $c = Math::BigInt->new("$c") unless ref($c) eq 'Math::BigInt';
  $n = Math::BigInt->new("$n") unless ref($n) eq 'Math::BigInt';
  $x = Math::BigInt->new("$x") unless ref($x) eq 'Math::BigInt';
  $z = Math::BigInt->new("$z") unless ref($z) eq 'Math::BigInt';

  croak "n must be >= 2" unless $n >= 2;
  $c->bmod($n);

  my $self = {
    c => $c,
    d => ($c + 2) >> 2,
    n => $n,
    x => $x,
    z => $z,
    f => $n-$n+1,
  };

  bless $self, $class;
  return $self;
}

sub _addx {
  my ($x1, $x2, $xin, $n) = @_;

  my $u = ($x2 - 1) * ($x1 + 1);
  my $v = ($x2 + 1) * ($x1 - 1);

  my $upv2 = ($u + $v) ** 2;
  my $umv2 = ($u - $v) ** 2;

  return ( $upv2 % $n, ($umv2*$xin) % $n );
}

sub _add3 {
  my ($x1, $z1, $x2, $z2, $xin, $zin, $n) = @_;

  my $u = ($x2 - $z2) * ($x1 + $z1);
  my $v = ($x2 + $z2) * ($x1 - $z1);

  my $upv2 = $u + $v;  $upv2->bmul($upv2);
  my $umv2 = $u - $v;  $umv2->bmul($umv2);

  $upv2->bmul($zin)->bmod($n);
  $umv2->bmul($xin)->bmod($n);
  return ($upv2, $umv2);
}

sub _double {
  my ($x, $z, $n, $d) = @_;

  my $u = $x + $z;   $u->bmul($u);
  my $v = $x - $z;   $v->bmul($v);

  my $w = $u - $v;
  my $t = $d * $w + $v;

  $u->bmul($v)->bmod($n);
  $w->bmul($t)->bmod($n);
  return ($u, $w);
}

sub mul {
  my ($self, $k) = @_;
  my $x = $self->{'x'};
  my $z = $self->{'z'};
  my $n = $self->{'n'};
  my $d = $self->{'d'};

  my ($x1, $x2, $z1, $z2);

  my $r = --$k;
  my $l = -1;
  while ($r != 1) { $r >>= 1; $l++ }
  if ($k & (1 << $l)) {
    ($x2, $z2) = _double($x, $z, $n, $d);
    ($x1, $z1) = _add3($x2, $z2, $x, $z, $x, $z, $n);
    ($x2, $z2) = _double($x2, $z2, $n, $d);
  } else {
    ($x1, $z1) = _double($x, $z, $n, $d);
    ($x2, $z2) = _add3($x, $z, $x1, $z1, $x, $z, $n);
  }
  $l--;
  while ($l >= 1) {
    if ($k & (1 << $l)) {
      ($x1, $z1) = _add3($x1, $z1, $x2, $z2, $x, $z, $n);
      ($x2, $z2) = _double($x2, $z2, $n, $d);
    } else {
      ($x2, $z2) = _add3($x2, $z2, $x1, $z1, $x, $z, $n);
      ($x1, $z1) = _double($x1, $z1, $n, $d);
    }
    $l--;
  }
  if ($k & 1) {
    ($x, $z) = _double($x2, $z2, $n, $d);
  } else {
    ($x, $z) = _add3($x2, $z2, $x1, $z1, $x, $z, $n);
  }

  $self->{'x'} = $x;
  $self->{'z'} = $z;
  return $self;
}

sub add {
  my ($self, $other) = @_;
  croak "add takes a EC point"
    unless ref($other) eq 'Math::Prime::Util::ECProjectivePoint';
  croak "second point is not on the same curve"
    unless $self->{'c'} == $other->{'c'} &&
           $self->{'n'} == $other->{'n'};

  ($self->{'x'}, $self->{'z'}) = _add3($self->{'x'}, $self->{'z'},
                                       $other->{'x'}, $other->{'z'},
                                       $self->{'x'}, $self->{'z'},
                                       $self->{'n'});
  return $self;
}

sub double {
  my ($self) = @_;
  ($self->{'x'}, $self->{'z'}) = _double($self->{'x'}, $self->{'z'}, $self->{'n'}, $self->{'d'});
  return $self;
}

#sub _extended_gcd {
#  my ($a, $b) = @_;
#  my $zero = $a-$a;
#  my ($x, $lastx, $y, $lasty) = ($zero, $zero+1, $zero+1, $zero);
#  while ($b != 0) {
#    my $q = int($a/$b);
#    ($a, $b) = ($b, $a % $b);
#    ($x, $lastx) = ($lastx - $q*$x, $x);
#    ($y, $lasty) = ($lasty - $q*$y, $y);
#  }
#  return ($a, $lastx, $lasty);
#}

sub normalize {
  my ($self) = @_;
  my $n = $self->{'n'};
  my $z = $self->{'z'};
  #my ($f, $u, undef) = _extended_gcd( $z, $n );
  my $f = Math::BigInt::bgcd( $z, $n );
  my $u = $z->copy->bmodinv($n);
  $self->{'x'} = ( $self->{'x'} * $u ) % $n;
  $self->{'z'} = $n-$n+1;
  $self->{'f'} = ($f * $self->{'f'}) % $n;
  return $self;
}

sub c { return shift->{'c'}; }
sub d { return shift->{'d'}; }
sub n { return shift->{'n'}; }
sub x { return shift->{'x'}; }
sub z { return shift->{'z'}; }
sub f { return shift->{'f'}; }

sub is_infinity {
  my $self = shift;
  return ($self->{'x'}->is_zero() && $self->{'z'}->is_one());
}

sub copy {
  my $self = shift;
  return Math::Prime::Util::ECProjectivePoint->new(
    $self->{'c'}, $self->{'n'}, $self->{'x'}, $self->{'z'});
}

1;

__END__


# ABSTRACT: Elliptic curve operations for projective points

=pod

=encoding utf8

=for stopwords mul

=for test_synopsis use v5.14;  my($c,$n,$k,$ECP2);


=head1 NAME

Math::Prime::Util::ECProjectivePoint - Elliptic curve operations for projective points


=head1 VERSION

Version 0.73


=head1 SYNOPSIS

  # Create a point on a curve (a,b,n) with coordinates 0,1
  my $ECP = Math::Prime::Util::ECProjectivePoint->new($c, $n, 0, 1);

  # scalar multiplication by $k.
  $ECP->mul($k);

  # add two points on the same curve
  $ECP->add($ECP2);

  say "P = O" if $ECP->is_infinity();

=head1 DESCRIPTION

This really should just be in Math::EllipticCurve.

To write.


=head1 FUNCTIONS

=head2 new

  $point = Math::Prime::Util::ECProjectivePoint->new(c, n, x, z);

Returns a new point on the curve defined by the Montgomery parameter c.

=head2 c

=head2 n

Returns the C<c>, C<d>, or C<n> values that describe the curve.

=head2 d

Returns the precalculated value of C<int( (c + 2) / 4 )>.

=head2 x

=head2 z

Returns the C<x> or C<z> values that define the point on the curve.

=head2 f

Returns a possible factor found after L</normalize>.

=head2 add

Takes another point on the same curve as an argument and adds it this point.

=head2 double

Double the current point on the curve.

=head2 mul

Takes an integer and performs scalar multiplication of the point.

=head2 is_infinity

Returns true if the point is (0,1), which is the point at infinity for
the affine coordinates.

=head2 copy

Returns a copy of the point.

=head2 normalize

Performs an extended GCD operation to make C<z=1>.  If a factor of C<n> is
found it is put in C<f>.


=head1 SEE ALSO

L<Math::EllipticCurve::Prime>

This really should just be in a L<Math::EllipticCurve> module.

=head1 AUTHORS

Dana Jacobsen E<lt>dana@acm.orgE<gt>


=head1 COPYRIGHT

Copyright 2012-2013 by Dana Jacobsen E<lt>dana@acm.orgE<gt>

This program is free software; you can redistribute it and/or modify it under the same terms as Perl itself.

=cut
