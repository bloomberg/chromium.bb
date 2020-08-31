package Crypt::Mode::OFB;

### BEWARE - GENERATED FILE, DO NOT EDIT MANUALLY!

use strict;
use warnings;
our $VERSION = '0.063';

use Crypt::Cipher;

sub encrypt {
  my ($self, $pt) = (shift, shift);
  local $SIG{__DIE__} = \&CryptX::_croak;
  $self->start_encrypt(@_)->add($pt);
}

sub decrypt {
  my ($self, $ct) = (shift, shift);
  local $SIG{__DIE__} = \&CryptX::_croak;
  $self->start_decrypt(@_)->add($ct);
}

sub CLONE_SKIP { 1 } # prevent cloning

1;

=pod

=head1 NAME

Crypt::Mode::OFB - Block cipher mode OFB [Output feedback]

=head1 SYNOPSIS

   use Crypt::Mode::OFB;
   my $m = Crypt::Mode::OFB->new('AES');

   #(en|de)crypt at once
   my $ciphertext = $m->encrypt($plaintext, $key, $iv);
   my $plaintext = $m->decrypt($ciphertext, $key, $iv);

   #encrypt more chunks
   $m->start_encrypt($key, $iv);
   my $ciphertext = $m->add('some data');
   $ciphertext .= $m->add('more data');

   #decrypt more chunks
   $m->start_decrypt($key, $iv);
   my $plaintext = $m->add($some_ciphertext);
   $plaintext .= $m->add($more_ciphertext);

=head1 DESCRIPTION

This module implements OFB cipher mode. B<NOTE:> it works only with ciphers from L<CryptX> (Crypt::Cipher::NNNN).

=head1 METHODS

=head2 new

 my $m = Crypt::Mode::OFB->new($name);
 #or
 my $m = Crypt::Mode::OFB->new($name, $cipher_rounds);

 # $name ............ one of 'AES', 'Anubis', 'Blowfish', 'CAST5', 'Camellia', 'DES', 'DES_EDE',
 #                    'KASUMI', 'Khazad', 'MULTI2', 'Noekeon', 'RC2', 'RC5', 'RC6',
 #                    'SAFERP', 'SAFER_K128', 'SAFER_K64', 'SAFER_SK128', 'SAFER_SK64',
 #                    'SEED', 'Skipjack', 'Twofish', 'XTEA', 'IDEA', 'Serpent'
 #                    simply any <NAME> for which there exists Crypt::Cipher::<NAME>
 # $cipher_rounds ... optional num of rounds for given cipher

=head2 encrypt

   my $ciphertext = $m->encrypt($plaintext, $key, $iv);

=head2 decrypt

   my $plaintext = $m->decrypt($ciphertext, $key, $iv);

=head2 start_encrypt

   $m->start_encrypt($key, $iv);

=head2 start_decrypt

   $m->start_decrypt($key, $iv);

=head2 add

   # in encrypt mode
   my $plaintext = $m->add($ciphertext);

   # in decrypt mode
   my $ciphertext = $m->add($plaintext);

=head1 SEE ALSO

=over

=item * L<CryptX|CryptX>, L<Crypt::Cipher>

=item * L<Crypt::Cipher::AES>, L<Crypt::Cipher::Blowfish>, ...

=item * L<https://en.wikipedia.org/wiki/Block_cipher_mode_of_operation#Output_feedback_.28OFB.29>

=back

=cut
