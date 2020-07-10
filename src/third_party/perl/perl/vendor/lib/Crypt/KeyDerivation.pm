package Crypt::KeyDerivation;

use strict;
use warnings;
our $VERSION = '0.063';

require Exporter; our @ISA = qw(Exporter); ### use Exporter 'import';
our %EXPORT_TAGS = ( all => [qw(pbkdf1 pbkdf2 hkdf hkdf_expand hkdf_extract)] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use CryptX;

1;

=pod

=head1 NAME

Crypt::KeyDerivation - PBKDF1, PBKDF2 and HKDF key derivation functions

=head1 SYNOPSIS

  use Crypt::KeyDerivation ':all';

  ### PBKDF1/2
  $derived_key1 = pbkdf1($password, $salt, $iteration_count, $hash_name, $len);
  $derived_key2 = pbkdf2($password, $salt, $iteration_count, $hash_name, $len);

  ### HKDF & co.
  $derived_key3 = hkdf($keying_material, $salt, $hash_name, $len, $info);
  $prk  = hkdf_extract($keying_material, $salt, $hash_name);
  $okm1 = hkdf_expand($prk, $hash_name, $len, $info);

=head1 DESCRIPTION

Provides an interface to Key derivation functions:

=over

=item * PBKDF1 and PBKDF according to PKCS#5 v2.0 L<https://tools.ietf.org/html/rfc2898|https://tools.ietf.org/html/rfc2898>

=item * HKDF (+ related) according to L<https://tools.ietf.org/html/rfc5869|https://tools.ietf.org/html/rfc5869>

=back

=head1 FUNCTIONS

=head2 pbkdf1

B<BEWARE:> if you are not sure, do not use C<pbkdf1> but rather choose C<pbkdf2>.

  $derived_key = pbkdf1($password, $salt, $iteration_count, $hash_name, $len);
  #or
  $derived_key = pbkdf1($password, $salt, $iteration_count, $hash_name);
  #or
  $derived_key = pbkdf1($password, $salt, $iteration_count);
  #or
  $derived_key = pbkdf1($password, $salt);

  # $password ......... input keying material  (password)
  # $salt ............. salt/nonce (expected length: 8)
  # $iteration_count .. optional, DEFAULT: 5000
  # $hash_name ........ optional, DEFAULT: 'SHA256'
  # $len .............. optional, derived key len, DEFAULT: 32

=head2 pbkdf2

  $derived_key = pbkdf2($password, $salt, $iteration_count, $hash_name, $len);
  #or
  $derived_key = pbkdf2($password, $salt, $iteration_count, $hash_name);
  #or
  $derived_key = pbkdf2($password, $salt, $iteration_count);
  #or
  $derived_key = pbkdf2($password, $salt);

  # $password ......... input keying material (password)
  # $salt ............. salt/nonce
  # $iteration_count .. optional, DEFAULT: 5000
  # $hash_name ........ optional, DEFAULT: 'SHA256'
  # $len .............. optional, derived key len, DEFAULT: 32

=head2 hkdf

  $okm2 = hkdf($password, $salt, $hash_name, $len, $info);
  #or
  $okm2 = hkdf($password, $salt, $hash_name, $len);
  #or
  $okm2 = hkdf($password, $salt, $hash_name);
  #or
  $okm2 = hkdf($password, $salt);

  # $password ... input keying material (password)
  # $salt ....... salt/nonce, if undef defaults to HashLen zero octets
  # $hash_name .. optional, DEFAULT: 'SHA256'
  # $len ........ optional, derived key len, DEFAULT: 32
  # $info ....... optional context and application specific information, DEFAULT: ''

=head2 hkdf_extract

  $prk  = hkdf_extract($password, $salt, $hash_name);
  #or
  $prk  = hkdf_extract($password, $salt, $hash_name);

  # $password ... input keying material (password)
  # $salt ....... salt/nonce, if undef defaults to HashLen zero octets
  # $hash_name .. optional, DEFAULT: 'SHA256'


=head2 hkdf_expand

  $okm = hkdf_expand($pseudokey, $hash_name, $len, $info);
  #or
  $okm = hkdf_expand($pseudokey, $hash_name, $len);
  #or
  $okm = hkdf_expand($pseudokey, $hash_name);
  #or
  $okm = hkdf_expand($pseudokey);

  # $pseudokey .. input keying material
  # $hash_name .. optional, DEFAULT: 'SHA256'
  # $len ........ optional, derived key len, DEFAULT: 32
  # $info ....... optional context and application specific information, DEFAULT: ''

=cut
