package Crypt::Mac::Pelican;

### BEWARE - GENERATED FILE, DO NOT EDIT MANUALLY!

use strict;
use warnings;
our $VERSION = '0.063';

use base qw(Crypt::Mac Exporter);
our %EXPORT_TAGS = ( all => [qw( pelican pelican_hex pelican_b64 pelican_b64u )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

1;

=pod

=head1 NAME

Crypt::Mac::Pelican - Message authentication code Pelican (AES based MAC)

=head1 SYNOPSIS

   ### Functional interface:
   use Crypt::Mac::Pelican qw( pelican pelican_hex );

   # calculate MAC from string/buffer
   $pelican_raw  = pelican($key, 'data buffer');
   $pelican_hex  = pelican_hex($key, 'data buffer');
   $pelican_b64  = pelican_b64($key, 'data buffer');
   $pelican_b64u = pelican_b64u($key, 'data buffer');

   ### OO interface:
   use Crypt::Mac::Pelican;

   $d = Crypt::Mac::Pelican->new($key);
   $d->add('any data');
   $d->addfile('filename.dat');
   $d->addfile(*FILEHANDLE);
   $result_raw  = $d->mac;     # raw bytes
   $result_hex  = $d->hexmac;  # hexadecimal form
   $result_b64  = $d->b64mac;  # Base64 form
   $result_b64u = $d->b64umac; # Base64 URL Safe form

=head1 DESCRIPTION

Provides an interface to the Pelican message authentication code (MAC) algorithm.

=head1 EXPORT

Nothing is exported by default.

You can export selected functions:

  use Crypt::Mac::Pelican qw(pelican pelican_hex );

Or all of them at once:

  use Crypt::Mac::Pelican ':all';

=head1 FUNCTIONS

=head2 pelican

Logically joins all arguments into a single string, and returns its Pelican message authentication code encoded as a binary string.

 $pelican_raw = pelican($key, 'data buffer');
 #or
 $pelican_raw = pelican($key, 'any data', 'more data', 'even more data');

=head2 pelican_hex

Logically joins all arguments into a single string, and returns its Pelican message authentication code encoded as a hexadecimal string.

 $pelican_hex = pelican_hex($key, 'data buffer');
 #or
 $pelican_hex = pelican_hex($key, 'any data', 'more data', 'even more data');

=head2 pelican_b64

Logically joins all arguments into a single string, and returns its Pelican message authentication code encoded as a Base64 string.

 $pelican_b64 = pelican_b64($key, 'data buffer');
 #or
 $pelican_b64 = pelican_b64($key, 'any data', 'more data', 'even more data');

=head2 pelican_b64u

Logically joins all arguments into a single string, and returns its Pelican message authentication code encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $pelican_b64url = pelican_b64u($key, 'data buffer');
 #or
 $pelican_b64url = pelican_b64u($key, 'any data', 'more data', 'even more data');

=head1 METHODS

=head2 new

 $d = Crypt::Mac::Pelican->new($key);

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

=item * L<http://eprint.iacr.org/2005/088.pdf>

=back

=cut
