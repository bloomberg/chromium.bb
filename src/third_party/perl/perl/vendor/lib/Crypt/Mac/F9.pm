package Crypt::Mac::F9;

### BEWARE - GENERATED FILE, DO NOT EDIT MANUALLY!

use strict;
use warnings;
our $VERSION = '0.063';

use base qw(Crypt::Mac Exporter);
our %EXPORT_TAGS = ( all => [qw( f9 f9_hex f9_b64 f9_b64u )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

1;

=pod

=head1 NAME

Crypt::Mac::F9 - Message authentication code F9

=head1 SYNOPSIS

   ### Functional interface:
   use Crypt::Mac::F9 qw( f9 f9_hex );

   # calculate MAC from string/buffer
   $f9_raw  = f9($cipher_name, $key, 'data buffer');
   $f9_hex  = f9_hex($cipher_name, $key, 'data buffer');
   $f9_b64  = f9_b64($cipher_name, $key, 'data buffer');
   $f9_b64u = f9_b64u($cipher_name, $key, 'data buffer');

   ### OO interface:
   use Crypt::Mac::F9;

   $d = Crypt::Mac::F9->new($cipher_name, $key);
   $d->add('any data');
   $d->addfile('filename.dat');
   $d->addfile(*FILEHANDLE);
   $result_raw  = $d->mac;     # raw bytes
   $result_hex  = $d->hexmac;  # hexadecimal form
   $result_b64  = $d->b64mac;  # Base64 form
   $result_b64u = $d->b64umac; # Base64 URL Safe form

=head1 DESCRIPTION

Provides an interface to the F9 message authentication code (MAC) algorithm.

=head1 EXPORT

Nothing is exported by default.

You can export selected functions:

  use Crypt::Mac::F9 qw(f9 f9_hex );

Or all of them at once:

  use Crypt::Mac::F9 ':all';

=head1 FUNCTIONS

=head2 f9

Logically joins all arguments into a single string, and returns its F9 message authentication code encoded as a binary string.

 $f9_raw = f9($cipher_name, $key, 'data buffer');
 #or
 $f9_raw = f9($cipher_name, $key, 'any data', 'more data', 'even more data');

=head2 f9_hex

Logically joins all arguments into a single string, and returns its F9 message authentication code encoded as a hexadecimal string.

 $f9_hex = f9_hex($cipher_name, $key, 'data buffer');
 #or
 $f9_hex = f9_hex($cipher_name, $key, 'any data', 'more data', 'even more data');

=head2 f9_b64

Logically joins all arguments into a single string, and returns its F9 message authentication code encoded as a Base64 string.

 $f9_b64 = f9_b64($cipher_name, $key, 'data buffer');
 #or
 $f9_b64 = f9_b64($cipher_name, $key, 'any data', 'more data', 'even more data');

=head2 f9_b64u

Logically joins all arguments into a single string, and returns its F9 message authentication code encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $f9_b64url = f9_b64u($cipher_name, $key, 'data buffer');
 #or
 $f9_b64url = f9_b64u($cipher_name, $key, 'any data', 'more data', 'even more data');

=head1 METHODS

=head2 new

 $d = Crypt::Mac::F9->new($cipher_name, $key);

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

=back

=cut
