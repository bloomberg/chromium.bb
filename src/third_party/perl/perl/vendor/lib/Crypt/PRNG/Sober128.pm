package Crypt::PRNG::Sober128;

use strict;
use warnings;
our $VERSION = '0.063';

use base qw(Crypt::PRNG Exporter);
our %EXPORT_TAGS = ( all => [qw(random_bytes random_bytes_hex random_bytes_b64 random_bytes_b64u random_string random_string_from rand irand)] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use CryptX;

{
  ### stolen from Bytes::Random::Secure
  my $RNG_object = undef;
  my $fetch_RNG = sub { # Lazily, instantiate the RNG object, but only once.
    $RNG_object = Crypt::PRNG::Sober128->new unless defined $RNG_object && ref($RNG_object) ne 'SCALAR';
    return $RNG_object;
  };
  sub rand               { return $fetch_RNG->()->double(@_) }
  sub irand              { return $fetch_RNG->()->int32(@_) }
  sub random_bytes       { return $fetch_RNG->()->bytes(@_) }
  sub random_bytes_hex   { return $fetch_RNG->()->bytes_hex(@_) }
  sub random_bytes_b64   { return $fetch_RNG->()->bytes_b64(@_) }
  sub random_bytes_b64u  { return $fetch_RNG->()->bytes_b64u(@_) }
  sub random_string_from { return $fetch_RNG->()->string_from(@_) }
  sub random_string      { return $fetch_RNG->()->string(@_) }
}


1;

=pod

=head1 NAME

Crypt::PRNG::Sober128 - Cryptographically secure PRNG based on Sober128 (stream cipher) algorithm

=head1 SYNOPSIS

   ### Functional interface:
   use Crypt::PRNG::Sober128 qw(random_bytes random_bytes_hex random_bytes_b64 random_string random_string_from rand irand);

   $octets = random_bytes(45);
   $hex_string = random_bytes_hex(45);
   $base64_string = random_bytes_b64(45);
   $base64url_string = random_bytes_b64u(45);
   $alphanumeric_string = random_string(30);
   $string = random_string_from('ACGT', 64);
   $floating_point_number_0_to_1 = rand;
   $floating_point_number_0_to_88 = rand(88);
   $unsigned_32bit_int = irand;

   ### OO interface:
   use Crypt::PRNG::Sober128;

   $prng = Crypt::PRNG::Sober128->new;
   #or
   $prng = Crypt::PRNG::Sober128->new("some data used for seeding PRNG");

   $octets = $prng->bytes(45);
   $hex_string = $prng->bytes_hex(45);
   $base64_string = $prng->bytes_b64(45);
   $base64url_string = $prng->bytes_b64u(45);
   $alphanumeric_string = $prng->string(30);
   $string = $prng->string_from('ACGT', 64);
   $floating_point_number_0_to_1 = rand;
   $floating_point_number_0_to_88 = rand(88);
   $unsigned_32bit_int = irand;

=head1 DESCRIPTION

Provides an interface to the Sober128 based pseudo random number generator

All methods and functions are the same as for L<Crypt::PRNG>.

=head1 FUNCTIONS

=head2 random_bytes

See L<Crypt::PRNG/random_bytes>.

=head2 random_bytes_hex

See L<Crypt::PRNG/random_bytes_hex>.

=head2 random_bytes_b64

See L<Crypt::PRNG/random_bytes_b64>.

=head2 random_bytes_b64u

See L<Crypt::PRNG/random_bytes_b64u>.

=head2 random_string

See L<Crypt::PRNG/random_string>.

=head2 random_string_from

See L<Crypt::PRNG/random_string_from>.

=head2 rand

See L<Crypt::PRNG/rand>.

=head2 irand

See L<Crypt::PRNG/irand>.

=head1 METHODS

=head2 new

See L<Crypt::PRNG/new>.

=head2 bytes

See L<Crypt::PRNG/bytes>.

=head2 bytes_hex

See L<Crypt::PRNG/bytes_hex>.

=head2 bytes_b64

See L<Crypt::PRNG/bytes_b64>.

=head2 bytes_b64u

See L<Crypt::PRNG/bytes_b64u>.

=head2 string

See L<Crypt::PRNG/string>.

=head2 string_from

See L<Crypt::PRNG/string_from>.

=head2 double

See L<Crypt::PRNG/double>.

=head2 int32

See L<Crypt::PRNG/int32>.

=head1 SEE ALSO

=over

=item * L<Crypt::PRNG>

=item * L<https://en.wikipedia.org/wiki/SOBER-128|https://en.wikipedia.org/wiki/SOBER-128>

=back

=cut
