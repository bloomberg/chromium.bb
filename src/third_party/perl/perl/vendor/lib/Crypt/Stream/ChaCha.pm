package Crypt::Stream::ChaCha;

use strict;
use warnings;
our $VERSION = '0.063';

use CryptX;

1;

=pod

=head1 NAME

Crypt::Stream::ChaCha - Stream cipher ChaCha

=head1 SYNOPSIS

   use Crypt::Stream::ChaCha;

   # encrypt
   $key = "1234567890123456";
   $iv  = "123456789012";
   $stream = Crypt::Stream::ChaCha->new($key, $iv);
   $ct = $stream->crypt("plain message");

   # decrypt
   $key = "1234567890123456";
   $iv  = "123456789012";
   $stream = Crypt::Stream::ChaCha->new($key, $iv);
   $pt = $stream->crypt($ct);

=head1 DESCRIPTION

Provides an interface to the ChaCha stream cipher.

=head1 METHODS

=head2 new

 $stream = Crypt::Stream::ChaCha->new($key, $iv);
 #or
 $stream = Crypt::Stream::ChaCha->new($key, $iv, $counter, $rounds);

 # $key     .. 32 or 16 bytes
 # $iv      .. 8 or 12 bytes
 # $counter .. initial counter value (DEFAULT: 0)
 # $rounds  .. rounds (DEFAULT: 20)

=head2 crypt

 $ciphertext = $stream->crypt($plaintext);
 #or
 $plaintext = $stream->crypt($ciphertext);

=head2 keystream

 $random_key = $stream->keystream($length);

=head2 clone

 $stream->clone();

=head1 SEE ALSO

=over

=item * L<Crypt::Stream::RC4>, L<Crypt::Stream::Sober128>, L<Crypt::Stream::Salsa20>, L<Crypt::Stream::Sosemanuk>

=item * L<https://tools.ietf.org/html/rfc7539>

=back

=cut
