package Crypt::Cipher::Blowfish;

### BEWARE - GENERATED FILE, DO NOT EDIT MANUALLY!

use strict;
use warnings;
our $VERSION = '0.063';

use base qw(Crypt::Cipher);

sub blocksize      { Crypt::Cipher::blocksize('Blowfish')      }
sub keysize        { Crypt::Cipher::keysize('Blowfish')        }
sub max_keysize    { Crypt::Cipher::max_keysize('Blowfish')    }
sub min_keysize    { Crypt::Cipher::min_keysize('Blowfish')    }
sub default_rounds { Crypt::Cipher::default_rounds('Blowfish') }

1;

=pod

=head1 NAME

Crypt::Cipher::Blowfish - Symmetric cipher Blowfish, key size: 64-448 bits (Crypt::CBC compliant)

=head1 SYNOPSIS

  ### example 1
  use Crypt::Mode::CBC;

  my $key = '...'; # length has to be valid key size for this cipher
  my $iv = '...';  # 16 bytes
  my $cbc = Crypt::Mode::CBC->new('Blowfish');
  my $ciphertext = $cbc->encrypt("secret data", $key, $iv);

  ### example 2 (slower)
  use Crypt::CBC;
  use Crypt::Cipher::Blowfish;

  my $key = '...'; # length has to be valid key size for this cipher
  my $iv = '...';  # 16 bytes
  my $cbc = Crypt::CBC->new( -cipher=>'Cipher::Blowfish', -key=>$key, -iv=>$iv );
  my $ciphertext = $cbc->encrypt("secret data");

=head1 DESCRIPTION

This module implements the Blowfish cipher. Provided interface is compliant with L<Crypt::CBC|Crypt::CBC> module.

B<BEWARE:> This module implements just elementary "one-block-(en|de)cryption" operation - if you want to
encrypt/decrypt generic data you have to use some of the cipher block modes - check for example
L<Crypt::Mode::CBC|Crypt::Mode::CBC>, L<Crypt::Mode::CTR|Crypt::Mode::CTR> or L<Crypt::CBC|Crypt::CBC> (which will be slower).

=head1 METHODS

=head2 new

 $c = Crypt::Cipher::Blowfish->new($key);
 #or
 $c = Crypt::Cipher::Blowfish->new($key, $rounds);

=head2 encrypt

 $ciphertext = $c->encrypt($plaintext);

=head2 decrypt

 $plaintext = $c->decrypt($ciphertext);

=head2 keysize

  $c->keysize;
  #or
  Crypt::Cipher::Blowfish->keysize;
  #or
  Crypt::Cipher::Blowfish::keysize;

=head2 blocksize

  $c->blocksize;
  #or
  Crypt::Cipher::Blowfish->blocksize;
  #or
  Crypt::Cipher::Blowfish::blocksize;

=head2 max_keysize

  $c->max_keysize;
  #or
  Crypt::Cipher::Blowfish->max_keysize;
  #or
  Crypt::Cipher::Blowfish::max_keysize;

=head2 min_keysize

  $c->min_keysize;
  #or
  Crypt::Cipher::Blowfish->min_keysize;
  #or
  Crypt::Cipher::Blowfish::min_keysize;

=head2 default_rounds

  $c->default_rounds;
  #or
  Crypt::Cipher::Blowfish->default_rounds;
  #or
  Crypt::Cipher::Blowfish::default_rounds;

=head1 SEE ALSO

=over

=item * L<CryptX|CryptX>, L<Crypt::Cipher>

=item * L<https://en.wikipedia.org/wiki/Blowfish_(cipher)>

=back

=cut
