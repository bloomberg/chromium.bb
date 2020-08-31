package Crypt::Cipher;

use strict;
use warnings;
our $VERSION = '0.063';

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use CryptX;

### the following methods/functions are implemented in XS:
# - new
# - DESTROY
# - blocksize
# - decrypt
# - default_rounds
# - encrypt
# - max_keysize
# - min_keysize

sub keysize { goto \&max_keysize; } # for Crypt::CBC compatibility

sub CLONE_SKIP { 1 } # prevent cloning

1;

=pod

=head1 NAME

Crypt::Cipher - Generic interface to cipher functions

=head1 SYNOPSIS

   #### example 1 (encrypting single block)
   use Crypt::Cipher;

   my $key = '...'; # length has to be valid key size for this cipher
   my $c = Crypt::Cipher->new('AES', $key);
   my $blocksize  = $c->blocksize;
   my $ciphertext = $c->encrypt('plain text block'); #encrypt 1 block
   my $plaintext  = $c->decrypt($ciphertext);         #decrypt 1 block

   ### example 2 (using CBC mode)
   use Crypt::Mode::CBC;

   my $key = '...'; # length has to be valid key size for this cipher
   my $iv = '...';  # 16 bytes
   my $cbc = Crypt::Mode::CBC->new('AES');
   my $ciphertext = $cbc->encrypt("secret data", $key, $iv);

   #### example 3 (compatibility with Crypt::CBC)
   use Crypt::CBC;
   use Crypt::Cipher;

   my $key = '...'; # length has to be valid key size for this cipher
   my $iv = '...';  # 16 bytes
   my $cipher = Crypt::Cipher('AES', $key);
   my $cbc = Crypt::CBC->new( -cipher=>$cipher, -iv=>$iv );
   my $ciphertext = $cbc->encrypt("secret data");

=head1 DESCRIPTION

Provides an interface to various symmetric cipher algorithms.

B<BEWARE:> This module implements just elementary "one-block-(en|de)cryption" operation - if you want to
encrypt/decrypt generic data you have to use some of the cipher block modes - check for example
L<Crypt::Mode::CBC|Crypt::Mode::CBC>, L<Crypt::Mode::CTR|Crypt::Mode::CTR> or L<Crypt::CBC|Crypt::CBC> (which will be slower).

=head1 METHODS

=head2 new

Constructor, returns a reference to the cipher object.

 ## basic scenario
 $d = Crypt::Cipher->new($name, $key);
 # $name = one of 'AES', 'Anubis', 'Blowfish', 'CAST5', 'Camellia', 'DES', 'DES_EDE',
 #                'KASUMI', 'Khazad', 'MULTI2', 'Noekeon', 'RC2', 'RC5', 'RC6',
 #                'SAFERP', 'SAFER_K128', 'SAFER_K64', 'SAFER_SK128', 'SAFER_SK64',
 #                'SEED', 'Skipjack', 'Twofish', 'XTEA', 'IDEA', 'Serpent'
 #                simply any <NAME> for which there exists Crypt::Cipher::<NAME>
 # $key = binary key (keysize should comply with selected cipher requirements)

 ## some of the ciphers (e.g. MULTI2, RC5, SAFER) allow one to set number of rounds
 $d = Crypt::Cipher->new('MULTI2', $key, $rounds);
 # $rounds = positive integer (should comply with selected cipher requirements)

=head2 encrypt

Encrypts $plaintext and returns the $ciphertext where $plaintext and $ciphertext should be of B<blocksize> bytes.

 $ciphertext = $d->encrypt($plaintext);

=head2 decrypt

Decrypts $ciphertext and returns the $plaintext where $plaintext and $ciphertext should be of B<blocksize> bytes.

 $plaintext = $d->encrypt($ciphertext);

=head2 keysize

Just an alias for B<max_keysize> (needed for L<Crypt::CBC|Crypt::CBC> compatibility).

=head2 max_keysize

Returns the maximal allowed key size (in bytes) for given cipher.

 $d->max_keysize;
 #or
 Crypt::Cipher->max_keysize('AES');
 #or
 Crypt::Cipher::max_keysize('AES');

=head2 min_keysize

Returns the minimal allowed key size (in bytes) for given cipher.

 $d->min_keysize;
 #or
 Crypt::Cipher->min_keysize('AES');
 #or
 Crypt::Cipher::min_keysize('AES');

=head2 blocksize

Returns block size (in bytes) for given cipher.

 $d->blocksize;
 #or
 Crypt::Cipher->blocksize('AES');
 #or
 Crypt::Cipher::blocksize('AES');

=head2 default_rounds

Returns default number of rounds for given cipher. NOTE: only some ciphers (e.g. MULTI2, RC5, SAFER) allow one to set number of rounds via new().

 $d->default_rounds;
 #or
 Crypt::Cipher->default_rounds('AES');
 #or
 Crypt::Cipher::default_rounds('AES');

=head1 SEE ALSO

=over

=item * L<CryptX|CryptX>

=item * Check subclasses like L<Crypt::Cipher::AES|Crypt::Cipher::AES>, L<Crypt::Cipher::Blowfish|Crypt::Cipher::Blowfish>, ...

=back

=cut
