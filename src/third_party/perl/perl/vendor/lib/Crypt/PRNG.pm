package Crypt::PRNG;

use strict;
use warnings;
our $VERSION = '0.063';

require Exporter; our @ISA = qw(Exporter); ### use Exporter 'import';
our %EXPORT_TAGS = ( all => [qw(random_bytes random_bytes_hex random_bytes_b64 random_bytes_b64u random_string random_string_from rand irand)] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use CryptX;

sub string {
  my ($self, $len) = @_;
  return $self->string_from("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", $len);
}

sub string_from {
  my ($self, $chars, $len) = @_;

  $len = 20 unless defined $len;
  return unless $len > 0;
  return unless length($chars) > 0;

  my @ch = split(//, $chars);
  my $max_index = $#ch;
  return if $max_index > 65535;

  my $mask;
  for my $n (1..31) {
    $mask = (1<<$n) - 1;
    last if $mask >= $max_index;
  }

  my $upck = ($max_index > 255) ? "n*" : "C*";
  my $l = $len * 2;

  my $rv = '';
  my @r;
  while (length $rv < $len) {
    @r = unpack($upck, $self->bytes($l)) if scalar @r == 0;
    my $i = (shift @r) & $mask;
    next if $i > $max_index;
    $rv .= $ch[$i];
  }
  return $rv;
}

sub CLONE_SKIP { 1 } # prevent cloning

### FUNCTIONS

{
  ### stolen from Bytes::Random::Secure
  #
  # Instantiate our random number generator(s) inside of a lexical closure,
  # limiting the scope of the RNG object so it can't be tampered with.
  my $RNG_object = undef;
  my $fetch_RNG = sub { # Lazily, instantiate the RNG object, but only once.
    $RNG_object = Crypt::PRNG->new unless defined $RNG_object && ref($RNG_object) ne 'SCALAR';
    return $RNG_object;
  };
  sub rand(;$)                { return $fetch_RNG->()->double(@_) }
  sub irand()                 { return $fetch_RNG->()->int32() }
  sub random_bytes($)         { return $fetch_RNG->()->bytes(@_) }
  sub random_bytes_hex($)     { return $fetch_RNG->()->bytes_hex(@_) }
  sub random_bytes_b64($)     { return $fetch_RNG->()->bytes_b64(@_) }
  sub random_bytes_b64u($)    { return $fetch_RNG->()->bytes_b64u(@_) }
  sub random_string_from($;$) { return $fetch_RNG->()->string_from(@_) }
  sub random_string(;$)       { return $fetch_RNG->()->string(@_) }
}

1;

=pod

=head1 NAME

Crypt::PRNG - Cryptographically secure random number generator

=head1 SYNOPSIS

   ### Functional interface:
   use Crypt::PRNG qw(random_bytes random_bytes_hex random_bytes_b64 random_bytes_b64u
                      random_string random_string_from rand irand);

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
   use Crypt::PRNG;

   $prng = Crypt::PRNG->new;
   #or
   $prng = Crypt::PRNG->new("RC4");
   #or
   $prng = Crypt::PRNG->new("RC4", "some data used for seeding PRNG");

   $octets = $prng->bytes(45);
   $hex_string = $prng->bytes_hex(45);
   $base64_string = $prng->bytes_b64(45);
   $base64url_string = $prng->bytes_b64u(45);
   $alphanumeric_string = $prng->string(30);
   $string = $prng->string_from('ACGT', 64);
   $floating_point_number_0_to_1 = $prng->double;
   $floating_point_number_0_to_88 = $prng->double(88);
   $unsigned_32bit_int = $prng->int32;

=head1 DESCRIPTION

Provides an interface to the ChaCha20 based pseudo random number generator (thread-safe and fork-safe).

=head1 FUNCTIONS

=head2 random_bytes

   $octets = random_bytes($length);

Returns C<$length> random octects.

=head2 random_bytes_hex

   $hex_string = random_bytes_hex($length);

Returns C<$length> random octects encoded as hexadecimal string.

=head2 random_bytes_b64

   $base64_string = random_bytes_b64($length);

Returns C<$length> random octects Base64 encoded.

=head2 random_bytes_b64u

   $base64url_string = random_bytes_b64u($length);

Returns C<$length> random octects Base64 URL Safe (RFC 4648 section 5) encoded.

=head2 random_string_from

   $string = random_string_from($range, $length);
   #e.g.
   $string = random_string_from("ABCD", 10);

Returns a random string made of C<$length> chars randomly chosen from C<$range> string.

=head2 random_string

   $alphanumeric_string = random_string($length);
   #or
   $alphanumeric_string = random_string;  # default length = 20

Similar to random_string_from, only C<$range> is fixed to C<'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789'>.

=head2 rand

   $n = rand;
   #or
   $n = rand($limit);

Returns a random floating point number from range C<[0,1)> (if called without parameter) or C<[0,$limit)>.

=head2 irand

   $i = irand;

Returns a random unsigned 32bit integer - range C<0 .. 0xFFFFFFFF>.

=head1 METHODS

=head2 new

   $prng = Crypt::PRNG->new;
   #or
   $prng = Crypt::PRNG->new($alg);
   #or
   $prng = Crypt::PRNG->new($alg, $seed);

   # $alg  ... algorithm name 'Frotuna' (DEFAULT), 'RC4', 'Sober128' or 'Yarrow'
   # $seed ... will be used as an initial entropy for seeding PRNG

If C<$seed> is not specified the PRNG is automatically seeded with 32bytes random data taken from C</dev/random> (UNIX) or C<CryptGenRandom> (Win32)

=head2 add_entropy

  $prng->add_entropy($random_data);
  #or
  $prng->add_entropy();

If called without parameter it uses 32bytes random data taken from C</dev/random> (UNIX) or C<CryptGenRandom> (Win32).

B<BEWARE:> you probably do not need this function at all as the module does automatic seeding on initialization as well as reseeding after fork and thread creation.

=head2 bytes

   $octets = $prng->bytes($length);

See L<random_bytes|/random_bytes>

=head2 bytes_hex

   $hex_string = $prng->bytes_hex($length);

See L<random_bytes_hex|/random_bytes_hex>

=head2 bytes_b64

   $base64_string = $prng->bytes_b64($length);

See L<random_bytes_b64|/random_bytes_b64>

=head2 bytes_b64u

   $base64url_string = $prng->bytes_b64u($length);

See L<random_bytes_b64u|/random_bytes_b64u>

=head2 string

   $alphanumeric_string = $prng->string($length);
   #or
   $alphanumeric_string = $prng->string;

See L<random_string|/random_string>

=head2 string_from

   $string = $prng->string_from($range, $length);

See L<random_string_from|/random_string_from>

=head2 double

   $n = $prng->double;
   #or
   $n = $prng->double($limit);

See L<rand|/rand>

=head2 int32

   $i = $prng->int32;

See L<irand|/irand>

=head1 SEE ALSO

L<Crypt::PRNG::Fortuna>, L<Crypt::PRNG::RC4>, L<Crypt::PRNG::Sober128>, L<Crypt::PRNG::Yarrow>

=cut
