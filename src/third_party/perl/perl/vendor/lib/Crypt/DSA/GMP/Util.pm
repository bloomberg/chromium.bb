package Crypt::DSA::GMP::Util;
use strict;
use warnings;

BEGIN {
  $Crypt::DSA::GMP::Util::AUTHORITY = 'cpan:DANAJ';
  $Crypt::DSA::GMP::Util::VERSION = '0.01';
}

use Carp qw( croak );
use Math::BigInt lib => "GMP";
use Crypt::Random::Seed;

use base qw( Exporter );
our @EXPORT_OK = qw( bitsize bin2mp mp2bin mod_inverse mod_exp randombytes makerandom makerandomrange );
our %EXPORT_TAGS = (all => [ @EXPORT_OK ]);

sub bitsize {
  my $n = shift;
  $n = Math::BigInt->new("$n") unless ref($n) eq 'Math::BigInt';
  length($n->as_bin) - 2;
}

# This is the os2ip function
sub bin2mp {
    my $s = shift;
    return Math::BigInt->new(0) if !defined $s || $s eq '';
    return Math::BigInt->from_hex('0x' . unpack("H*", $s));
}

# This is the i2osp function
sub mp2bin {
    my $p = shift;
    my $res = '';
    if (ref($p) ne 'Math::BigInt' && $p <= ~0) {
      do {
        $res = chr($p & 0xFF) . $res;
        $p >>= 8;
      } while $p;
    } else {
      $p = Math::BigInt->new("$p") unless ref($p) eq 'Math::BigInt';
      my $hex = $p->as_hex;
      $hex =~ s/^0x0*//;
      substr($hex, 0, 0, '0') if length($hex) % 2;
      $res = pack("H*", $hex);
    }
    $res;
}

sub mod_exp {
    my($a, $exp, $n) = @_;
    $a->copy->bmodpow($exp, $n);
}

sub mod_inverse {
    my($a, $n) = @_;
    $a->copy->bmodinv($n);
}

{
  my ($crs, $crs_best);
  sub _setup_rng {
    $crs_best = Crypt::Random::Seed->new();
    $crs = ($crs_best->is_blocking())
           ? Crypt::Random::Seed->new(NonBlocking=>1)
           : $crs_best;
  }
  sub randombytes {
    my($bytes, $keygen) = @_;
    _setup_rng() unless defined $crs;
    my $src = ($keygen) ? $crs_best : $crs;
    return $src->random_bytes($bytes);
  }
}

# Generate uniform random number in range [2^(bits-1),2^bits-1]
sub makerandom {
  my %param = @_;
  my ($bits, $is_keygen) = ( $param{Size}, $param{KeyGen} );
  croak "makerandom must have Size >= 1" unless defined $bits && $bits > 0;
  return Math::BigInt->bone if $bits == 1;

  my $randbits = $bits - 1;
  my $randbytes = int(($randbits+7)/8);
  my $randbinary = unpack("B*", randombytes( $randbytes, $is_keygen ));
  return Math::BigInt->from_bin( '0b1' . substr($randbinary,0,$randbits) );
}

# Generate uniform random number in range [0, $max]
sub makerandomrange {
  my %param = @_;
  my ($max, $is_keygen) = ( $param{Max}, $param{KeyGen} );
  croak "makerandomrange must have a Max > 0" unless defined $max && $max > 0;
  $max = Math::BigInt->new("$max") unless ref($max) eq 'Math::BigInt';
  my $range = $max->copy->binc;
  my $bits = length($range->as_bin) - 2;
  my $bytes = 1 + int(($bits+7)/8);
  my $rmax = Math::BigInt->bone->blsft(8*$bytes)->bdec();
  my $overflow = $rmax - ($rmax % $range);
  my $U;
  do {
    $U = Math::BigInt->from_hex( '0x' . unpack("H*", randombytes($bytes,$is_keygen)) );
  } while $U >= $overflow;
  $U->bmod($range);  # U is randomly in [0, k*$range-1] for some k.
  return $U;
}

1;
__END__

=pod

=for stopwords mod_exp($a makerandom makerandomrange

=head1 NAME

Crypt::DSA::GMP::Util - DSA Utility functions

=head1 SYNOPSIS

    use Crypt::DSA::GMP::Util qw( func1 func2 ... );

=head1 DESCRIPTION

L<Crypt::DSA::GMP::Util> contains a set of exportable utility functions
used through the L<Crypt::DSA::GMP> module.

=head2 bitsize($n)

Returns the number of bits in the integer I<$n>.

=head2 bin2mp($string)

Given a string I<$string> of any length, treats the string as a
base-256 representation of an integer, and returns that integer.

=head2 mp2bin($int)

Given an integer I<$int> (maybe a L<Math::BigInt> object),
returns an octet string representation (e.g. a string where
each byte is a base-256 digit of the integer).

=head2 mod_exp($a, $exp, $n)

Computes $a ^ $exp mod $n and returns the value.

=head2 mod_inverse($a, $n)

Computes the multiplicative inverse of $a mod $n and returns the
value.

=head2 randombytes($n)

Returns I<$n> random bytes from the entropy source.  The entropy
source is a L<Crypt::Random::Seed> source.

An optional boolean second argument indicates whether the data
is being used for key generation, hence the best possible
randomness is used.  If this argument is not present or is false,
then the best non-blocking source will be used.

=head2 makerandom

  $n = makerandom(Size => 512);

Takes a I<Size> argument and creates a random L<Math::BigInt>
with exactly that number of bits using data from L</randombytes>.
The high order bit will always be set.

Also takes an optional I<KeyGen> argument that is given to
L</randombytes>.

=head2 makerandomrange

  $n = makerandomrange(Max => $max);  # 0 <= $n <= $max

Returns a L<Math::BigInt> uniformly randomly selected between
I<0> and I<$max>.  Random data is provided by L</randombytes>.

Also takes an optional I<KeyGen> argument that is given to
L</randombytes>.

=head1 AUTHOR & COPYRIGHTS

See L<Crypt::DSA::GMP> for author, copyright, and license information.

=cut
