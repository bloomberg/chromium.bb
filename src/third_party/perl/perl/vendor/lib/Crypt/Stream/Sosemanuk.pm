package Crypt::Stream::Sosemanuk;

use strict;
use warnings;
our $VERSION = '0.063';

use CryptX;

1;

=pod

=head1 NAME

Crypt::Stream::Sosemanuk - Stream cipher Sosemanuk

=head1 SYNOPSIS

   use Crypt::Stream::Sosemanuk;

   # encrypt
   $key = "1234567890123456";
   $iv  = "123456789012";
   $stream = Crypt::Stream::Sosemanuk->new($key, $iv);
   $ct = $stream->crypt("plain message");

   # decrypt
   $key = "1234567890123456";
   $iv  = "123456789012";
   $stream = Crypt::Stream::Sosemanuk->new($key, $iv);
   $pt = $stream->crypt($ct);

=head1 DESCRIPTION

Provides an interface to the Sosemanuk stream cipher.

=head1 METHODS

=head2 new

 $stream = Crypt::Stream::Sosemanuk->new($key, $iv);
 # $key .. keylen must be multiple of 4 bytes
 # $iv  .. ivlen must be multiple of 4 bytes (OPTIONAL)

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

=item * L<Crypt::Stream::RC4>, L<Crypt::Stream::ChaCha>, L<Crypt::Stream::Salsa20>, L<Crypt::Stream::Sober128>

=item * L<https://en.wikipedia.org/wiki/SOSEMANUK>

=back

=cut
