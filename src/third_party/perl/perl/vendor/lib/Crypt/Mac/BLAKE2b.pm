package Crypt::Mac::BLAKE2b;

### BEWARE - GENERATED FILE, DO NOT EDIT MANUALLY!

use strict;
use warnings;
our $VERSION = '0.063';

use base qw(Crypt::Mac Exporter);
our %EXPORT_TAGS = ( all => [qw( blake2b blake2b_hex blake2b_b64 blake2b_b64u )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

1;

=pod

=head1 NAME

Crypt::Mac::BLAKE2b - Message authentication code BLAKE2b MAC (RFC 7693)

=head1 SYNOPSIS

   ### Functional interface:
   use Crypt::Mac::BLAKE2b qw( blake2b blake2b_hex );

   # calculate MAC from string/buffer
   $blake2b_raw  = blake2b($size, $key, 'data buffer');
   $blake2b_hex  = blake2b_hex($size, $key, 'data buffer');
   $blake2b_b64  = blake2b_b64($size, $key, 'data buffer');
   $blake2b_b64u = blake2b_b64u($size, $key, 'data buffer');

   ### OO interface:
   use Crypt::Mac::BLAKE2b;

   $d = Crypt::Mac::BLAKE2b->new($size, $key);
   $d->add('any data');
   $d->addfile('filename.dat');
   $d->addfile(*FILEHANDLE);
   $result_raw  = $d->mac;     # raw bytes
   $result_hex  = $d->hexmac;  # hexadecimal form
   $result_b64  = $d->b64mac;  # Base64 form
   $result_b64u = $d->b64umac; # Base64 URL Safe form

=head1 DESCRIPTION

Provides an interface to the BLAKE2b message authentication code (MAC) algorithm.

=head1 EXPORT

Nothing is exported by default.

You can export selected functions:

  use Crypt::Mac::BLAKE2b qw(blake2b blake2b_hex );

Or all of them at once:

  use Crypt::Mac::BLAKE2b ':all';

=head1 FUNCTIONS

=head2 blake2b

Logically joins all arguments into a single string, and returns its BLAKE2b message authentication code encoded as a binary string.

 $blake2b_raw = blake2b($size, $key, 'data buffer');
 #or
 $blake2b_raw = blake2b($size, $key, 'any data', 'more data', 'even more data');

=head2 blake2b_hex

Logically joins all arguments into a single string, and returns its BLAKE2b message authentication code encoded as a hexadecimal string.

 $blake2b_hex = blake2b_hex($size, $key, 'data buffer');
 #or
 $blake2b_hex = blake2b_hex($size, $key, 'any data', 'more data', 'even more data');

=head2 blake2b_b64

Logically joins all arguments into a single string, and returns its BLAKE2b message authentication code encoded as a Base64 string.

 $blake2b_b64 = blake2b_b64($size, $key, 'data buffer');
 #or
 $blake2b_b64 = blake2b_b64($size, $key, 'any data', 'more data', 'even more data');

=head2 blake2b_b64u

Logically joins all arguments into a single string, and returns its BLAKE2b message authentication code encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $blake2b_b64url = blake2b_b64u($size, $key, 'data buffer');
 #or
 $blake2b_b64url = blake2b_b64u($size, $key, 'any data', 'more data', 'even more data');

=head1 METHODS

=head2 new

 $d = Crypt::Mac::BLAKE2b->new($size, $key);

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

=item * L<https://tools.ietf.org/html/rfc7693>

=back

=cut
