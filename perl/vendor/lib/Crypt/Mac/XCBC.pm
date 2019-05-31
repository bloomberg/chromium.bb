package Crypt::Mac::XCBC;

### BEWARE - GENERATED FILE, DO NOT EDIT MANUALLY!

use strict;
use warnings;
our $VERSION = '0.063';

use base qw(Crypt::Mac Exporter);
our %EXPORT_TAGS = ( all => [qw( xcbc xcbc_hex xcbc_b64 xcbc_b64u )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

1;

=pod

=head1 NAME

Crypt::Mac::XCBC - Message authentication code XCBC (RFC 3566)

=head1 SYNOPSIS

   ### Functional interface:
   use Crypt::Mac::XCBC qw( xcbc xcbc_hex );

   # calculate MAC from string/buffer
   $xcbc_raw  = xcbc($cipher_name, $key, 'data buffer');
   $xcbc_hex  = xcbc_hex($cipher_name, $key, 'data buffer');
   $xcbc_b64  = xcbc_b64($cipher_name, $key, 'data buffer');
   $xcbc_b64u = xcbc_b64u($cipher_name, $key, 'data buffer');

   ### OO interface:
   use Crypt::Mac::XCBC;

   $d = Crypt::Mac::XCBC->new($cipher_name, $key);
   $d->add('any data');
   $d->addfile('filename.dat');
   $d->addfile(*FILEHANDLE);
   $result_raw  = $d->mac;     # raw bytes
   $result_hex  = $d->hexmac;  # hexadecimal form
   $result_b64  = $d->b64mac;  # Base64 form
   $result_b64u = $d->b64umac; # Base64 URL Safe form

=head1 DESCRIPTION

Provides an interface to the XCBC message authentication code (MAC) algorithm.

=head1 EXPORT

Nothing is exported by default.

You can export selected functions:

  use Crypt::Mac::XCBC qw(xcbc xcbc_hex );

Or all of them at once:

  use Crypt::Mac::XCBC ':all';

=head1 FUNCTIONS

=head2 xcbc

Logically joins all arguments into a single string, and returns its XCBC message authentication code encoded as a binary string.

 $xcbc_raw = xcbc($cipher_name, $key, 'data buffer');
 #or
 $xcbc_raw = xcbc($cipher_name, $key, 'any data', 'more data', 'even more data');

=head2 xcbc_hex

Logically joins all arguments into a single string, and returns its XCBC message authentication code encoded as a hexadecimal string.

 $xcbc_hex = xcbc_hex($cipher_name, $key, 'data buffer');
 #or
 $xcbc_hex = xcbc_hex($cipher_name, $key, 'any data', 'more data', 'even more data');

=head2 xcbc_b64

Logically joins all arguments into a single string, and returns its XCBC message authentication code encoded as a Base64 string.

 $xcbc_b64 = xcbc_b64($cipher_name, $key, 'data buffer');
 #or
 $xcbc_b64 = xcbc_b64($cipher_name, $key, 'any data', 'more data', 'even more data');

=head2 xcbc_b64u

Logically joins all arguments into a single string, and returns its XCBC message authentication code encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $xcbc_b64url = xcbc_b64u($cipher_name, $key, 'data buffer');
 #or
 $xcbc_b64url = xcbc_b64u($cipher_name, $key, 'any data', 'more data', 'even more data');

=head1 METHODS

=head2 new

 $d = Crypt::Mac::XCBC->new($cipher_name, $key);

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

=item * L<https://www.ietf.org/rfc/rfc3566.txt>

=back

=cut
