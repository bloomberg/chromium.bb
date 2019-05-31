package Crypt::AuthEnc::GCM;

use strict;
use warnings;
our $VERSION = '0.063';

require Exporter; our @ISA = qw(Exporter); ### use Exporter 'import';
our %EXPORT_TAGS = ( all => [qw( gcm_encrypt_authenticate gcm_decrypt_verify )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use CryptX;

sub CLONE_SKIP { 1 } # prevent cloning

1;

=pod

=head1 NAME

Crypt::AuthEnc::GCM - Authenticated encryption in GCM mode

=head1 SYNOPSIS

 ### OO interface
 use Crypt::AuthEnc::GCM;

 # encrypt and authenticate
 my $ae = Crypt::AuthEnc::GCM->new("AES", $key, $iv);
 $ae->adata_add('additional_authenticated_data1');
 $ae->adata_add('additional_authenticated_data2');
 my $ct = $ae->encrypt_add('data1');
 $ct .= $ae->encrypt_add('data2');
 $ct .= $ae->encrypt_add('data3');
 my $tag = $ae->encrypt_done();

 # decrypt and verify
 my $ae = Crypt::AuthEnc::GCM->new("AES", $key, $iv);
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
 use Crypt::AuthEnc::GCM qw(gcm_encrypt_authenticate gcm_decrypt_verify);

 my ($ciphertext, $tag) = gcm_encrypt_authenticate('AES', $key, $iv, $adata, $plaintext);
 my $plaintext = gcm_decrypt_verify('AES', $key, $iv, $adata, $ciphertext, $tag);

=head1 DESCRIPTION

Galois/Counter Mode (GCM) - provides encryption and authentication.

=head1 EXPORT

Nothing is exported by default.

You can export selected functions:

  use Crypt::AuthEnc::GCM qw(gcm_encrypt_authenticate gcm_decrypt_verify);

=head1 FUNCTIONS

=head2 gcm_encrypt_authenticate

 my ($ciphertext, $tag) = gcm_encrypt_authenticate($cipher, $key, $iv, $adata, $plaintext);

 # $cipher .. 'AES' or name of any other cipher with 16-byte block len
 # $key ..... AES key of proper length (128/192/256bits)
 # $iv ...... initialization vector
 # $adata ... additional authenticated data

=head2 gcm_decrypt_verify

 my $plaintext = gcm_decrypt_verify($cipher, $key, $iv, $adata, $ciphertext, $tag);
 # on error returns undef

=head1 METHODS

=head2 new

 my $ae = Crypt::AuthEnc::GCM->new($cipher, $key);
 #or
 my $ae = Crypt::AuthEnc::GCM->new($cipher, $key, $iv);

 # $cipher .. 'AES' or name of any other cipher
 # $key ..... encryption key of proper length
 # $iv ...... initialization vector (optional, you can set it later via iv_add method)

=head2 iv_add

Set initialization vector (IV).

 $ae->iv_add($iv_data);                        #can be called multiple times

=head2 adata_add

Add B<additional authenticated data>.
Can be called B<after> all C<iv_add> calls but before the first C<encrypt_add> or C<decrypt_add>.

 $ae->adata_add($aad_data);                    # can be called multiple times

=head2 encrypt_add

 $ciphertext = $ae->encrypt_add($data);        # can be called multiple times

=head2 encrypt_done

 $tag = $ae->encrypt_done();                   # returns $tag value

=head2 decrypt_add

 $plaintext = $ae->decrypt_add($ciphertext);   # can be called multiple times

=head2 decrypt_done

 my $tag = $ae->decrypt_done;           # returns $tag value
 #or
 my $result = $ae->decrypt_done($tag);  # returns 1 (success) or 0 (failure)

=head2 reset

 $ae->reset;

=head2 clone

 my $ae_new = $ae->clone;

=head1 SEE ALSO

=over

=item * L<CryptX|CryptX>, L<Crypt::AuthEnc::CCM|Crypt::AuthEnc::CCM>, L<Crypt::AuthEnc::EAX|Crypt::AuthEnc::EAX>, L<Crypt::AuthEnc::OCB|Crypt::AuthEnc::OCB>

=item * L<https://en.wikipedia.org/wiki/Galois/Counter_Mode>

=back

=cut
