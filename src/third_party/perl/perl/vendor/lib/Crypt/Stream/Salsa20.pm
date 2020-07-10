package Crypt::Stream::Salsa20;

use strict;
use warnings;
our $VERSION = '0.063';

use CryptX;

1;

=pod

=head1 NAME

Crypt::Stream::Salsa20 - Stream cipher Salsa20

=head1 SYNOPSIS

   use Crypt::Stream::Salsa20;

   # encrypt
   $key = "1234567890123456";
   $iv  = "12345678";
   $stream = Crypt::Stream::Salsa20->new($key, $iv);
   $ct = $stream->crypt("plain message");

   # decrypt
   $key = "1234567890123456";
   $iv  = "12345678";
   $stream = Crypt::Stream::Salsa20->new($key, $iv);
   $pt = $stream->crypt($ct);

=head1 DESCRIPTION

Provides an interface to the Salsa20 stream cipher.

=head1 METHODS

=head2 new

 $stream = Crypt::Stream::Salsa20->new($key, $iv);
 #or
 $stream = Crypt::Stream::Salsa20->new($key, $iv, $counter, $rounds);

 # $key     .. 32 or 16 bytes
 # $iv      .. 8 bytes
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

=item * L<Crypt::Stream::ChaCha>, L<Crypt::Stream::RC4>, L<Crypt::Stream::Sober128>, L<Crypt::Stream::Sosemanuk>

=back

=cut
