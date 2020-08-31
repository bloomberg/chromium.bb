package Imager::Matrix2d;
use strict;
use vars qw($VERSION);
use Scalar::Util qw(reftype looks_like_number);
use Carp qw(croak);

$VERSION = "1.012";

=head1 NAME

  Imager::Matrix2d - simple wrapper for matrix construction

=head1 SYNOPSIS

  use Imager::Matrix2d;
  $m1 = Imager::Matrix2d->identity;
  $m2 = Imager::Matrix2d->rotate(radians=>$angle, x=>$cx, y=>$cy);
  $m3 = Imager::Matrix2d->translate(x=>$dx, y=>$dy);
  $m4 = Imager::Matrix2d->shear(x=>$sx, y=>$sy);
  $m5 = Imager::Matrix2d->reflect(axis=>$axis);
  $m6 = Imager::Matrix2d->scale(x=>$xratio, y=>$yratio);
  $m8 = Imager::Matric2d->matrix($v11, $v12, $v13,
                                 $v21, $v22, $v23,
                                 $v31, $v32, $v33);
  $m6 = $m1 * $m2;
  $m7 = $m1 + $m2;
  use Imager::Matrix2d qw(:handy);
  # various m2d_* functions imported 
  # where m2d_(.*) calls Imager::Matrix2d->$1()

=head1 DESCRIPTION

This class provides a simple wrapper around a reference to an array of
9 coefficients, treated as a matrix:

 [ 0, 1, 2,
   3, 4, 5,
   6, 7, 8 ]

Most of the methods in this class are constructors.  The others are
overloaded operators.

Note that since Imager represents images with y increasing from top to
bottom, rotation angles are clockwise, rather than counter-clockwise.

=over

=cut

use vars qw(@EXPORT_OK %EXPORT_TAGS @ISA);
@ISA = 'Exporter';
require 'Exporter.pm';
@EXPORT_OK = qw(m2d_rotate m2d_identity m2d_translate m2d_shear 
                m2d_reflect m2d_scale);
%EXPORT_TAGS =
  (
   handy=> [ qw(m2d_rotate m2d_identity m2d_translate m2d_shear 
                m2d_reflect m2d_scale) ],
  );

use overload 
  '*' => \&_mult,
  '+' => \&_add,
  '""'=>\&_string,
  "eq" => \&_eq;

=item identity()

Returns the identity matrix.

=cut

sub identity {
  return bless [ 1, 0, 0,
                 0, 1, 0,
                 0, 0, 1 ], $_[0];
}

=item rotate(radians=>$angle)

=item rotate(degrees=>$angle)

Creates a matrix that rotates around the origin, or around the point
(x,y) if the 'x' and 'y' parameters are provided.

=cut

sub rotate {
  my ($class, %opts) = @_;
  my $angle;

  if (defined $opts{radians}) {
    $angle = $opts{radians};
  }
  elsif (defined $opts{degrees}) {
    $angle = $opts{degrees} * 3.1415926535 / 180;
  }
  else {
    $Imager::ERRSTR = "degrees or radians parameter required";
    return undef;
  }

  if ($opts{'x'} || $opts{'y'}) {
    $opts{'x'} ||= 0;
    $opts{'y'} ||= 0;
    return $class->translate('x'=>$opts{'x'}, 'y'=>$opts{'y'})
      * $class->rotate(radians=>$angle)
        * $class->translate('x'=>-$opts{'x'}, 'y'=>-$opts{'y'});
  }
  else {
    my $sin = sin($angle);
    my $cos = cos($angle);
    return bless [ $cos, -$sin, 0,
                   $sin,  $cos, 0,
                   0,     0,    1 ], $class;
  }
}

=item translate(x=>$dx, y=>$dy)

=item translate(x=>$dx)

=item translate(y=>$dy)

Translates by the specify amounts.

=cut

sub translate {
  my ($class, %opts) = @_;

  if (defined $opts{'x'} || defined $opts{'y'}) {
    my $x = $opts{'x'} || 0;
    my $y = $opts{'y'} || 0;
    return bless [ 1, 0, $x,
                   0, 1, $y,
                   0, 0, 1 ], $class;
  }

  $Imager::ERRSTR = 'x or y parameter required';
  return undef;
}

=item shear(x=>$sx, y=>$sy)

=item shear(x=>$sx)

=item shear(y=>$sy)

Shear by the given amounts.

=cut
sub shear {
  my ($class, %opts) = @_;

  if (defined $opts{'x'} || defined $opts{'y'}) {
    return bless [ 1,             $opts{'x'}||0, 0,
                   $opts{'y'}||0, 1,             0,
                   0,             0,             1 ], $class;
  }
  $Imager::ERRSTR = 'x and y parameters required';
  return undef;
}

=item reflect(axis=>$axis)

Reflect around the given axis, either 'x' or 'y'.

=item reflect(radians=>$angle)

=item reflect(degrees=>$angle)

Reflect around a line drawn at the given angle from the origin.

=cut

sub reflect {
  my ($class, %opts) = @_;
  
  if (defined $opts{axis}) {
    my $result = $class->identity;
    if ($opts{axis} eq "y") {
      $result->[0] = -$result->[0];
    }
    elsif ($opts{axis} eq "x") {
      $result->[4] = -$result->[4];
    }
    else {
      $Imager::ERRSTR = 'axis must be x or y';
      return undef;
    }

    return $result;
  }
  my $angle;
  if (defined $opts{radians}) {
    $angle = $opts{radians};
  }
  elsif (defined $opts{degrees}) {
    $angle = $opts{degrees} * 3.1415926535 / 180;
  }
  else {
    $Imager::ERRSTR = 'axis, degrees or radians parameter required';
    return undef;
  }

  # fun with matrices
  return $class->rotate(radians=>-$angle) * $class->reflect(axis=>'x') 
    * $class->rotate(radians=>$angle);
}

=item scale(x=>$xratio, y=>$yratio)

Scales at the given ratios.

You can also specify a center for the scaling with the C<cx> and C<cy>
parameters.

=cut

sub scale {
  my ($class, %opts) = @_;

  if (defined $opts{'x'} || defined $opts{'y'}) {
    $opts{'x'} = 1 unless defined $opts{'x'};
    $opts{'y'} = 1 unless defined $opts{'y'};
    if ($opts{cx} || $opts{cy}) {
      return $class->translate('x'=>-$opts{cx}, 'y'=>-$opts{cy})
        * $class->scale('x'=>$opts{'x'}, 'y'=>$opts{'y'})
          * $class->translate('x'=>$opts{cx}, 'y'=>$opts{cy});
    }
    else {
      return bless [ $opts{'x'}, 0,          0,
                     0,          $opts{'y'}, 0,
                     0,          0,          1 ], $class;
    }
  }
  else {
    $Imager::ERRSTR = 'x or y parameter required';
    return undef;
  }
}

=item matrix($v11, $v12, $v13, $v21, $v22, $v23, $v31, $v32, $v33)

Create a matrix with custom coefficients.

=cut

sub matrix {
  my ($class, @self) = @_;

  if (@self == 9) {
    return bless \@self, $class;
  }
  else {
    $Imager::ERRSTR = "9 coefficients required";
    return;
  }
}

=item transform($x, $y)

Transform a point the same way matrix_transform does.

=cut

sub transform {
  my ($self, $x, $y) = @_;

  my $sz = $x * $self->[6] + $y * $self->[7] + $self->[8];
  my ($sx, $sy);
  if (abs($sz) > 0.000001) {
    $sx = ($x * $self->[0] + $y * $self->[1] + $self->[2]) / $sz;
    $sy = ($x * $self->[3] + $y * $self->[4] + $self->[5]) / $sz;
  }
  else {
    $sx = $sy = 0;
  }

  return ($sx, $sy);
}

=item compose(matrix...)

Compose several matrices together for use in transformation.

For example, for three matrices:

  my $out = Imager::Matrix2d->compose($m1, $m2, $m3);

is equivalent to:

  my $out = $m3 * $m2 * $m1;

Returns the identity matrix if no parameters are supplied.

May return the supplied matrix if only one matrix is supplied.

=cut

sub compose {
  my ($class, @in) = @_;

  @in
    or return $class->identity;

  my $out = pop @in;
  for my $m (reverse @in) {
    $out = $out * $m;
  }

  return $out;
}

=item _mult()

Implements the overloaded '*' operator.  Internal use.

Currently both the left and right-hand sides of the operator must be
an Imager::Matrix2d.

When composing a matrix for transformation you should multiply the
matrices in the reverse order of the transformations:

  my $shear = Imager::Matrix2d->shear(x => 0.1);
  my $rotate = Imager::Matrix2d->rotate(degrees => 45);
  my $shear_then_rotate = $rotate * $shear;

or use the compose method:

  my $shear_then_rotate = Imager::Matrix2d->compose($shear, $rotate);

=cut

sub _mult {
  my ($left, $right, $order) = @_;

  if (ref($right)) {
    if (reftype($right) eq "ARRAY") {
      @$right == 9
	or croak "9 elements required in array ref";
      if ($order) {
	($left, $right) = ($right, $left);
      }
      my @result;
      for my $i (0..2) {
	for my $j (0..2) {
	  my $accum = 0;
	  for my $k (0..2) {
	    $accum += $left->[3*$i + $k] * $right->[3*$k + $j];
	  }
	  $result[3*$i+$j] = $accum;
	}
      }
      return bless \@result, __PACKAGE__;
    }
    else {
      croak "multiply by array ref or number";
    }
  }
  elsif (defined $right && looks_like_number($right)) {
    my @result = map $_ * $right, @$left;

    return bless \@result, __PACKAGE__;
  }
  else {
    # something we don't handle
    croak "multiply by array ref or number";
  }
}

=item _add()

Implements the overloaded binary '+' operator.

Currently both the left and right sides of the operator must be
Imager::Matrix2d objects.

=cut
sub _add {
  my ($left, $right, $order) = @_;

  if (ref($right) && UNIVERSAL::isa($right, __PACKAGE__)) {
    my @result;
    for (0..8) {
      push @result, $left->[$_] + $right->[$_];
    }
    
    return bless \@result, __PACKAGE__;
  }
  else {
    return undef;
  }
}

=item _string()

Implements the overloaded stringification operator.

This returns a string containing 3 lines of text with no terminating
newline.

I tried to make it fairly nicely formatted.  You might disagree :)

=cut

sub _string {
  my ($m) = @_;

  my $maxlen = 0;
  for (@$m[0..8]) {
    if (length() > $maxlen) {
      $maxlen = length;
    }
  }
  $maxlen <= 9 or $maxlen = 9;

  my @left = ('[ ', '  ', '  ');
  my @right = ("\n", "\n", ']');
  my $out;
  my $width = $maxlen+2;
  for my $i (0..2) {
    $out .= $left[$i];
    for my $j (0..2) {
      my $val = $m->[$i*3+$j];
      if (length $val > 9) {
        $val = sprintf("%9f", $val);
        if ($val =~ /\./ && $val !~ /e/i) {
          $val =~ s/0+$//;
          $val =~ s/\.$//;
        }
        $val =~ s/^\s//;
      }
      $out .= sprintf("%-${width}s", "$val, ");
    }
    $out =~ s/ +\Z/ /;
    $out .= $right[$i];
  }
  $out;
}

=item _eq

Implement the overloaded equality operator.

Provided for older perls that don't handle magic auto generation of eq
from "".

=cut

sub _eq {
  my ($left, $right) = @_;

  return $left . "" eq $right . "";
}

=back

The following functions are shortcuts to the various constructors.

These are not methods.

You can import these methods with:

  use Imager::Matrix2d ':handy';

=over

=item m2d_identity

=item m2d_rotate()

=item m2d_translate()

=item m2d_shear()

=item m2d_reflect()

=item m2d_scale()

=back

=cut

sub m2d_identity {
  return __PACKAGE__->identity;
}

sub m2d_rotate {
  return __PACKAGE__->rotate(@_);
}

sub m2d_translate {
  return __PACKAGE__->translate(@_);
}

sub m2d_shear {
  return __PACKAGE__->shear(@_);
}

sub m2d_reflect {
  return __PACKAGE__->reflect(@_);
}

sub m2d_scale {
  return __PACKAGE__->scale(@_);
}

1;

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=head1 BUGS

Needs a way to invert a matrix.

=head1 SEE ALSO

Imager(3), Imager::Font(3)

http://imager.perl.org/

=cut
