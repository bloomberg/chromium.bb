package Crypt::Mac::Poly1305;

### BEWARE - GENERATED FILE, DO NOT EDIT MANUALLY!

use strict;
use warnings;
our $VERSION = '0.063';

use base qw(Crypt::Mac Exporter);
our %EXPORT_TAGS = ( all => [qw( poly1305 poly1305_hex poly1305_b64 poly1305_b64u )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

1;

=pod

=head1 NAME

Crypt::Mac::Poly1305 - Message authentication code Poly1305 (RFC 7539)

=head1 SYNOPSIS

   ### Functional interface:
   use Crypt::Mac::Poly1305 qw( poly1305 poly1305_hex );

   # calculate MAC from string/buffer
   $poly1305_raw  = poly1305($key, 'data buffer');
   $poly1305_hex  = poly1305_hex($key, 'data buffer');
   $poly1305_b64  = poly1305_b64($key, 'data buffer');
   $poly1305_b64u = poly1305_b64u($key, 'data buffer');

   ### OO interface:
   use Crypt::Mac::Poly1305;

   $d = Crypt::Mac::Poly1305->new($key);
   $d->add('any data');
   $d->addfile('filename.dat');
   $d->addfile(*FILEHANDLE);
   $result_raw  = $d->mac;     # raw bytes
   $result_hex  = $d->hexmac;  # hexadecimal form
   $result_b64  = $d->b64mac;  # Base64 form
   $result_b64u = $d->b64umac; # Base64 URL Safe form

=head1 DESCRIPTION

Provides an interface to the Poly1305 message authentication code (MAC) algorithm.

=head1 EXPORT

Nothing is exported by default.

You can export selected functions:

  use Crypt::Mac::Poly1305 qw(poly1305 poly1305_hex );

Or all of them at once:

  use Crypt::Mac::Poly1305 ':all';

=head1 FUNCTIONS

=head2 poly1305

Logically joins all arguments into a single string, and returns its Poly1305 message authentication code encoded as a binary string.

 $poly1305_raw = poly1305($key, 'data buffer');
 #or
 $poly1305_raw = poly1305($key, 'any data', 'more data', 'even more data');

=head2 poly1305_hex

Logically joins all arguments into a single string, and returns its Poly1305 message authentication code encoded as a hexadecimal string.

 $poly1305_hex = poly1305_hex($key, 'data buffer');
 #or
 $poly1305_hex = poly1305_hex($key, 'any data', 'more data', 'even more data');

=head2 poly1305_b64

Logically joins all arguments into a single string, and returns its Poly1305 message authentication code encoded as a Base64 string.

 $poly1305_b64 = poly1305_b64($key, 'data buffer');
 #or
 $poly1305_b64 = poly1305_b64($key, 'any data', 'more data', 'even more data');

=head2 poly1305_b64u

Logically joins all arguments into a single string, and returns its Poly1305 message authentication code encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $poly1305_b64url = poly1305_b64u($key, 'data buffer');
 #or
 $poly1305_b64url = poly1305_b64u($key, 'any data', 'more data', 'even more data');

=head1 METHODS

=head2 new

 $d = Crypt::Mac::Poly1305->new($key);

=head2 clone

 $d->clone();

=head2 reset

 $d->reset();

=head2 add

 $d->add('any data');
 #or
 $d->add('any data', 'more data', 'even more data');

=head2 addfile

 $d->addfile('filename.dat');
 #or
 $d->addfile(*FILEHANDLE);

=head2 mac

 $result_raw = $d->mac();

=head2 hexmac

 $result_hex = $d->hexmac();

=head2 b64mac

 $result_b64 = $d->b64mac();

=head2 b64umac

 $result_b64url = $d->b64umac();

=head1 SEE ALSO

=over

=item * L<CryptX|CryptX>

=item * L<https://www.ietf.org/rfc/rfc7539.txt>

=back

=cut
