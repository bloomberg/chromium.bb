package Crypt::AuthEnc::OCB;

use strict;
use warnings;
our $VERSION = '0.063';

require Exporter; our @ISA = qw(Exporter); ### use Exporter 'import';
our %EXPORT_TAGS = ( all => [qw( ocb_encrypt_authenticate ocb_decrypt_verify )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use CryptX;

# obsolete, only for backwards compatibility
sub aad_add { goto &adata_add }
sub blocksize { return 16 }

sub CLONE_SKIP { 1 } # prevent cloning

1;

=pod

=head1 NAME

Crypt::AuthEnc::OCB - Authenticated encryption in OCBv3 mode

=head1 SYNOPSIS

 ### OO interface
 use Crypt::AuthEnc::OCB;

 # encrypt and authenticate
 my $ae = Crypt::AuthEnc::OCB->new("AES", $key, $nonce, $tag_len);
 $ae->adata_add('additional_authenticated_data1');
 $ae->adata_add('additional_authenticated_data2');
 my $ct = $ae->encrypt_add('data1');
 $ct .= $ae->encrypt_add('data2');
 $ct .= $ae->encrypt_add('data3');
 $ct .= $ae->encrypt_last('rest of data');
 my $tag = $ae->encrypt_done();

 # decrypt and verify
 my $ae = Crypt::AuthEnc::OCB->new("AES", $key, $nonce, $tag_len);
 $ae->adata_add('additional_authenticated_data1');
 $ae->adata_add('additional_authenticated_data2');
 my $pt = $ae->decrypt_add('ciphertext1');
 $pt .= $ae->decrypt_add('ciphertext2');
 $pt .= $ae->decrypt_add('ciphertext3');
 $pt .= $ae->decrypt_last('rest of data');
 my $tag = $ae->decrypt_done();
 die "decrypt failed" unless $tag eq $expected_tag;

 #or
 my $result = $ae->decrypt_done($expected_tag); # 0 or 1

 ### functional interface
 use Crypt::AuthEnc::OCB qw(ocb_encrypt_authenticate ocb_decrypt_verify);

 my ($ciphertext, $tag) = ocb_encrypt_authenticate('AES', $key, $nonce, $adata, $tag_len, $plaintext);
 my $plaintext = ocb_decrypt_verify('AES', $key, $nonce, $adata, $ciphertext, $tag);

=head1 DESCRIPTION

This module implements OCB v3 according to L<https://tools.ietf.org/html/rfc7253>

=head1 EXPORT

Nothing is exported by default.

You can export selected functions:

  use Crypt::AuthEnc::OCB qw(ocb_encrypt_authenticate ocb_decrypt_verify);

=head1 FUNCTIONS

=head2 ocb_encrypt_authenticate

 my ($ciphertext, $tag) = ocb_encrypt_authenticate($cipher, $key, $nonce, $adata, $tag_len, $plaintext);

 # $cipher .. 'AES' or name of any other cipher with 16-byte block len
 # $key ..... AES key of proper length (128/192/256bits)
 # $nonce ... unique nonce/salt (no need to keep it secret)
 # $adata ... additional authenticated data
 # $tag_len . required length of output tag

=head2 ocb_decrypt_verify

  my $plaintext = ocb_decrypt_verify($cipher, $key, $nonce, $adata, $ciphertext, $tag);
  # on error returns undef

=head1 METHODS

=head2 new

 my $ae = Crypt::AuthEnc::OCB->new($cipher, $key, $nonce, $tag_len);

 # $cipher .. 'AES' or name of any other cipher with 16-byte block len
 # $key ..... AES key of proper length (128/192/256bits)
 # $nonce ... unique nonce/salt (no need to keep it secret)
 # $tag_len . required length of output tag

=head2 adata_add

 $ae->adata_add($adata);                        #can be called multiple times

=head2 encrypt_add

 $ciphertext = $ae->encrypt_add($data);         # can be called multiple times

 #BEWARE: size of $data has to be multiple of blocklen (16 for AES)

=head2 encrypt_last

 $ciphertext = $ae->encrypt_last($data);

=head2 encrypt_done

 $tag = $ae->encrypt_done();                    # returns $tag value

=head2 decrypt_add

 $plaintext = $ae->decrypt_add($ciphertext);    # can be called multiple times

 #BEWARE: size of $ciphertext has to be multiple of blocklen (16 for AES)

=head2 decrypt_last

 $plaintext = $ae->decrypt_last($data);

=head2 decrypt_done

 my $tag = $ae->decrypt_done;           # returns $tag value
 #or
 my $result = $ae->decrypt_done($tag);  # returns 1 (success) or 0 (failure)

=head2 clone

 my $ae_new = $ae->clone;

=head1 SEE ALSO

=over

=item * L<CryptX|CryptX>, L<Crypt::AuthEnc::CCM|Crypt::AuthEnc::CCM>, L<Crypt::AuthEnc::GCM|Crypt::AuthEnc::GCM>, L<Crypt::AuthEnc::EAX|Crypt::AuthEnc::EAX>

=item * L<https://en.wikipedia.org/wiki/OCB_mode>

=item * L<https://tools.ietf.org/html/rfc7253>

=back

=cut
