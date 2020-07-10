package Crypt::Stream::RC4;

use strict;
use warnings;
our $VERSION = '0.063';

use CryptX;

1;

=pod

=head1 NAME

Crypt::Stream::RC4 - Stream cipher RC4

=head1 SYNOPSIS

   use Crypt::Stream::RC4;

   # encrypt
   $key = "1234567890123456";
   $stream = Crypt::Stream::RC4->new($key);
   $ct = $stream->crypt("plain message");

   # decrypt
   $key = "1234567890123456";
   $stream = Crypt::Stream::RC4->new($key);
   $pt = $stream->crypt($ct);

=head1 DESCRIPTION

Provides an interface to the RC4 stream cipher.

=head1 METHODS

=head2 new

 $stream = Crypt::Stream::RC4->new($key);
 # $key .. length 5-256 bytes (40 - 2048 bits)

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

=item * L<Crypt::Stream::ChaCha>, L<Crypt::Stream::Sober128>, L<Crypt::Stream::Salsa20>, L<Crypt::Stream::Sosemanuk>

=item * L<https://en.wikipedia.org/wiki/RC4_cipher|https://en.wikipedia.org/wiki/RC4_cipher>

=back

=cut
