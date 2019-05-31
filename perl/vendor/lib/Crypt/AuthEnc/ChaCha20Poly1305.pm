package Crypt::AuthEnc::ChaCha20Poly1305;

use strict;
use warnings;
our $VERSION = '0.063';

require Exporter; our @ISA = qw(Exporter); ### use Exporter 'import';
our %EXPORT_TAGS = ( all => [qw( chacha20poly1305_encrypt_authenticate chacha20poly1305_decrypt_verify )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use CryptX;

sub CLONE_SKIP { 1 } # prevent cloning

1;

=pod

=head1 NAME

Crypt::AuthEnc::ChaCha20Poly1305 - Authenticated encryption in ChaCha20-Poly1305 mode

=head1 SYNOPSIS

 ### OO interface
 use Crypt::AuthEnc::ChaCha20Poly1305;

 # encrypt and authenticate
 my $ae = Crypt::AuthEnc::ChaCha20Poly1305->new($key, $iv);
 $ae->adata_add('additional_authenticated_data1');
 $ae->adata_add('additional_authenticated_data2');
 my $ct = $ae->encrypt_add('data1');
 $ct .= $ae->encrypt_add('data2');
 $ct .= $ae->encrypt_add('data3');
 my $tag = $ae->encrypt_done();

 # decrypt and verify
 my $ae = Crypt::AuthEnc::ChaCha20Poly1305->new($key, $iv);
 $ae->adata_add('additional_authenticated_data1');
 $ae->adata_add('additional_authenticated_data2');
 my $pt = $ae->decrypt_add('ciphertext1');
 $pt .= $ae->decrypt_add('ciphertext2');
 $pt .= $ae->decrypt_add('ciphertext3');
 my $tag = $ae->decrypt_done();
 die "decrypt failed" unless $tag eq $expected_tag;

 #or
 my $result = $ae->decrypt_done($expected_tag); # 0 or 1

 ### functional interface
 use Crypt::AuthEnc::ChaCha20Poly1305 qw(chacha20poly1305_encrypt_authenticate chacha20poly1305_decrypt_verify);

 my ($ciphertext, $tag) = chacha20poly1305_encrypt_authenticate($key, $iv, $adata, $plaintext);
 my $plaintext = chacha20poly1305_decrypt_verify($key, $iv, $adata, $ciphertext, $tag);

=head1 DESCRIPTION

Provides encryption and authentication based on ChaCha20 + Poly1305 as defined in RFC 7539 - L<https://tools.ietf.org/html/rfc7539>

=head1 EXPORT

Nothing is exported by default.

You can export selected functions:

  use Crypt::AuthEnc::ChaCha20Poly1305 qw(chacha20poly1305_encrypt_authenticate chacha20poly1305_decrypt_verify);

=head1 FUNCTIONS

=head2 chacha20poly1305_encrypt_authenticate

 my ($ciphertext, $tag) = chacha20poly1305_encrypt_authenticate($key, $iv, $adata, $plaintext);

 # $key ..... key of proper length (128 or 256 bits / 16 or 32 bytes)
 # $iv ...... initialization vector (64 or 96 bits / 8 or 12 bytes)
 # $adata ... additional authenticated data (optional)

=head2 chacha20poly1305_decrypt_verify

 my $plaintext = chacha20poly1305_decrypt_verify($key, $iv, $adata, $ciphertext, $tag);
 # on error returns undef

=head1 METHODS

=head2 new

 my $ae = Crypt::AuthEnc::ChaCha20Poly1305->new($key, $iv);

 # $key ..... encryption key of proper length (128 or 256 bits / 16 or 32 bytes)
 # $iv ...... initialization vector (64 or 96 bits / 8 or 12 bytes)

=head2 adata_add

Add B<additional authenticated data>.
Can be called before the first C<encrypt_add> or C<decrypt_add>;

 $ae->adata_add($aad_data);                     # can be called multiple times

=head2 encrypt_add

 $ciphertext = $ae->encrypt_add($data);         # can be called multiple times

=head2 encrypt_done

 $tag = $ae->encrypt_done();                    # returns $tag value

=head2 decrypt_add

 $plaintext = $ae->decrypt_add($ciphertext);    # can be called multiple times

=head2 decrypt_done

 my $tag = $ae->decrypt_done;           # returns $tag value
 #or
 my $result = $ae->decrypt_done($tag);  # returns 1 (success) or 0 (failure)

=head2 set_iv

 my $ae = Crypt::AuthEnc::ChaCha20Poly1305->new($key)->set_iv($iv);
 # $iv ...... initialization vector (64 or 96 bits / 8 or 12 bytes)

=head2 set_iv_rfc7905

See L<https://tools.ietf.org/html/rfc7905>

 my $ae = Crypt::AuthEnc::ChaCha20Poly1305->new($key)->set_iv_rfc7905($iv, $seqnum);
 # $iv ...... initialization vector (96 bits / 12 bytes)
 # $seqnum .. 64bit integer (sequence number)

=head2 clone

 my $ae_new = $ae->clone;

=head1 SEE ALSO

=over

=item * L<CryptX|CryptX>, L<Crypt::AuthEnc::GCM|Crypt::AuthEnc::GCM>, L<Crypt::AuthEnc::CCM|Crypt::AuthEnc::CCM>, L<Crypt::AuthEnc::EAX|Crypt::AuthEnc::EAX>, L<Crypt::AuthEnc::OCB|Crypt::AuthEnc::OCB>

=item * L<https://tools.ietf.org/html/rfc7539>

=back

=cut
