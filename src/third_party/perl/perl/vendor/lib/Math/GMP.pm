package Math::GMP;

# Math::GMP, a Perl module for high-speed arbitrary size integer
# calculations
# Copyright (C) 2000-2008 James H. Turner
# Copyright (C) 2008-2009 Greg Sabino Mullane

# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.

# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.

# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# You can contact the author at chip@redhat.com, chipt@cpan.org, or by mail:

# Chip Turner
# Red Hat Inc.
# 2600 Meridian Park Blvd
# Durham, NC 27713

use strict;
use warnings;
use 5.006;
use Carp;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK $AUTOLOAD);

use overload
	'""'  =>   \&stringify,
	'0+'  =>   \&intify,

	'<=>'  =>  \&op_spaceship,
	'cmp'  =>  \&op_cmp,

	'+'   =>   \&op_add,
	'-'   =>   \&op_sub,

	'&'   =>   \&op_and,
	'^'   =>   \&op_xor,
	'|'   =>   \&op_or,

	'%'   =>   \&op_mod,
	'**'   =>  \&op_pow,
	'*'   =>   \&op_mul,
	'/'   =>   \&op_div;

require Exporter;
require DynaLoader;
require AutoLoader;

@ISA = qw(Exporter DynaLoader);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

our $VERSION = '2.06';

sub AUTOLOAD {
	# This AUTOLOAD is used to 'autoload' constants from the constant()
	# XS function.  If a constant is not found then control is passed
	# to the AUTOLOAD in AutoLoader.

	my $constname;
	($constname = $AUTOLOAD) =~ s/.*:://;
	croak '& not defined' if $constname eq 'constant';
	my $val = constant($constname, @_ ? $_[0] : 0);
	if ($! != 0) {
		if ($! =~ /Invalid/) {
			$AutoLoader::AUTOLOAD = $AUTOLOAD;
			goto &AutoLoader::AUTOLOAD;
		} else {
			croak "Your vendor has not defined Math::GMP macro $constname";
		}
	}
	no strict 'refs';			## no critic
	*$AUTOLOAD = sub () { $val };
	goto &$AUTOLOAD;
}

bootstrap Math::GMP $VERSION;

use strict;
sub import {
	shift;
	return unless @_;
	die "unknown import: @_" unless @_ == 1 and $_[0] eq ':constant';
	overload::constant integer => sub { Math::GMP->new(shift) };
	return;
}


sub new {
	my $class = shift;
	my $ival = shift || 0;
	my $base = shift;

	$ival =~ s/^\+//;
	$ival =~ s/[ _]//g;
	my $ret;
	if ($base) {
		$ret = Math::GMP::new_from_scalar_with_base($ival, $base);
	} else {
		$ival = 0 if $ival =~ /[^\d\-xA-Fa-f]/;
		$ret = Math::GMP::new_from_scalar($ival);
	}

	return $ret;
}

BEGIN
	{
		*DESTROY = \&Math::GMP::destroy;
	}

sub add {
	croak 'add: not enough arguments, two required' if @_ < 2;

	my $ret = Math::GMP->new(0);
	add_to_self($ret, shift) while @_;

	return $ret;
}

sub stringify {
	return Math::GMP::stringify_gmp($_[0]);
}

sub intify {
	return Math::GMP::intify_gmp($_[0]);
}

sub promote {
	return $_[0] if ref $_[0] eq 'Math::GMP';
	return Math::GMP::new_from_scalar($_[0] || 0);
}

sub gcd {
	return gcd_two(promote(shift), promote(shift));
}

sub bgcd {
	return gcd_two(promote(shift), promote(shift));
}

sub legendre {
	return gmp_legendre(promote(shift), promote(shift));
}

sub jacobi {
	return gmp_jacobi(promote(shift), promote(shift));
}

sub probab_prime {
	my $x = shift;
	my $reps = shift;
	return gmp_probab_prime(promote($x), $reps);
}

sub op_add {
	my ($n, $m) = @_;
	($n, $m) = ($m, $n) if $_[2];
	return add_two(promote($n), promote($m));
}

sub op_sub {
	my ($n, $m) = @_;
	($n, $m) = ($m, $n) if $_[2];
	return sub_two(promote($n), promote($m));
}

sub op_mul {
	my ($n, $m) = @_;
	($n, $m) = ($m, $n) if $_[2];
	return mul_two(promote($n), promote($m));
}

sub op_div {
	my ($n, $m) = @_;
	($n, $m) = ($m, $n) if $_[2];
	return div_two(promote($n), promote($m));
}

sub bdiv {
	return bdiv_two(promote(shift), promote(shift));
}


sub op_mod {
	my ($n, $m) = @_;
	($n, $m) = ($m, $n) if $_[2];
	return mod_two(promote($n), promote($m));
}



sub op_cmp {
	my ($n, $m) = @_;
	($n, $m) = ($m, $n) if $_[2];
	return cmp_two(stringify(promote($n)), stringify(promote($m)));
}

sub op_spaceship {
	my ($n, $m) = @_;
	($n, $m) = ($m, $n) if $_[2];
	my $x = cmp_two(promote($n), promote($m));
	return $x < 0 ? -1 : $x > 0 ? 1 : 0;
}

sub op_pow {
	my ($m, $n) = @_;
	($n, $m) = ($m, $n) if $_[2];
	return pow_two(promote($m), int($n));
}


sub op_and {
	my ($n, $m) = @_;
	($n, $m) = ($m, $n) if $_[2];
	return and_two(promote($n), promote($m));
}

sub op_xor {
	my ($n, $m) = @_;
	($n, $m) = ($m, $n) if $_[2];
	return xor_two(promote($n), promote($m));
}

sub op_or {
	my ($n, $m) = @_;
	($n, $m) = ($m, $n) if $_[2];
	return or_two(promote($n), promote($m));
}

sub bior {
	return or_two(promote(shift), promote(shift));
}

sub band {
	return and_two(promote(shift), promote(shift));
}

sub bxor {
	return xor_two(promote(shift), promote(shift));
}

sub bfac {
	return gmp_fac(int(shift));
}

sub fibonacci {
	return gmp_fib(int(shift));
}

__END__

=head1 NAME

Math::GMP - High speed arbitrary size integer math

=head1 SYNOPSIS

  use Math::GMP;
  my $n = new Math::GMP 2;

  $n = $n ** (256*1024);
  $n = $n - 1;
  print "n is now $n\n";

=head1 DESCRIPTION

Math::GMP was designed to be a drop-in replacement both for
Math::BigInt and for regular integer arithmetic.  Unlike BigInt,
though, Math::GMP uses the GNU gmp library for all of its
calculations, as opposed to straight Perl functions.  This can result
in speed improvements.

The downside is that this module requires a C compiler to install -- a
small tradeoff in most cases. Also, this module is not 100% compatible
to Math::BigInt.

A Math::GMP object can be used just as a normal numeric scalar would
be -- the module overloads most of the normal arithmetic operators to
provide as seamless an interface as possible. However, if you need a
perfect interface, you can do the following:

  use Math::GMP qw(:constant);

  $n = 2 ** (256 * 1024);
  print "n is $n\n";

This would fail without the ':constant' since Perl would use normal
doubles to compute the 250,000 bit number, and thereby overflow it
into meaninglessness (smaller exponents yield less accurate data due
to floating point rounding).

=head1 METHODS

Although the non-overload interface is not complete, the following
functions do exist:

=head2 new

  $x = Math::GMP->new(123);

Creates a new Math::GMP object from the passed string or scalar.

  $x = Math::GMP->new('abcd', 36);

Creates a new Math::GMP object from the first parameter which should
be represented in the base specified by the second parameter.

=head2 bfac

  $x = Math::GMP->new(5);
  $x->bfac();      # 1*2*3*4*5 = 120

Calculates the factorial of $x and modifies $x to contain the result.

=head2 band

  $x = Math::GMP->new(6);
  $x->band(3);      # 0b110 & 0b11 = 1

Calculates the bit-wise AND of it's two arguments and modifies the first
argument.

=head2 bxor

  $x = Math::GMP->new(6);
  $x->bxor(3);      # 0b110 & 0b11 = 0b101

Calculates the bit-wise XOR of it's two arguments and modifies the first
argument.

=head2 bior

  $x = Math::GMP->new(6);
  $x->bior(3);      # 0b110 & 0b11 = 0b111

Calculates the bit-wise OR of it's two arguments and modifies the first
argument.

=head2 bgcd

  $x = Math::GMP->new(6);
  $x->bgcd(4);      # 6 / 2 = 2, 4 / 2 = 2 => 2

Calculates the Greatest Common Divisior of it's two arguments and returns the result.

=head2 legendre

=head2 jacobi

=head2 fibonacci

  $x = Math::GMP->fibonacci(16);

Calculates the n'th number in the Fibonacci sequence.

=head2 probab_prime

  $x = Math::GMP->new(7);
  $x->probab_prime(10);

Probabilistically Determines if the number is a prime. Argument is the number
of checks to perform. Returns 0 if the number is definitely not a prime,
1 if it may be, and 2 if it is definitely is a prime.

=head1 BUGS

As of version 1.0, Math::GMP is mostly compatible with the old
Math::BigInt version. It is not a full replacement for the rewritten
Math::BigInt versions, though. See the L<SEE ALSO section|SEE ALSO>
on how to achieve to use Math::GMP and retain full compatibility to
Math::BigInt.

There are some slight incompatibilities, such as output of positive
numbers not being prefixed by a '+' sign.  This is intentional.

There are also some things missing, and not everything might work as
expected.

=head1 SEE ALSO

Math::BigInt has a new interface to use a different library than the
default pure Perl implementation. You can use, for instance, Math::GMP
with it:

  use Math::BigInt lib => 'GMP';

If Math::GMP is not installed, it will fall back to it's own Perl
implementation.

See L<Math::BigInt> and L<Math::BigInt::GMP> or
L<Math::BigInt::Pari> or L<Math::BigInt::BitVect>.

=head1 AUTHOR

Chip Turner <chip@redhat.com>, based on the old Math::BigInt by Mark Biggar
and Ilya Zakharevich.  Further extensive work provided by Tels 
<tels@bloodgate.com>.


=cut
