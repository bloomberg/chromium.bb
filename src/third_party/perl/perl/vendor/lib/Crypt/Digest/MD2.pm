package Crypt::Digest::MD2;

### BEWARE - GENERATED FILE, DO NOT EDIT MANUALLY!

use strict;
use warnings;
our $VERSION = '0.063';

use base qw(Crypt::Digest Exporter);
our %EXPORT_TAGS = ( all => [qw( md2 md2_hex md2_b64 md2_b64u md2_file md2_file_hex md2_file_b64 md2_file_b64u )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use Crypt::Digest;

sub hashsize                { Crypt::Digest::hashsize('MD2')             }
sub md2             { Crypt::Digest::digest_data('MD2', @_)      }
sub md2_hex         { Crypt::Digest::digest_data_hex('MD2', @_)  }
sub md2_b64         { Crypt::Digest::digest_data_b64('MD2', @_)  }
sub md2_b64u        { Crypt::Digest::digest_data_b64u('MD2', @_) }
sub md2_file        { Crypt::Digest::digest_file('MD2', @_)      }
sub md2_file_hex    { Crypt::Digest::digest_file_hex('MD2', @_)  }
sub md2_file_b64    { Crypt::Digest::digest_file_b64('MD2', @_)  }
sub md2_file_b64u   { Crypt::Digest::digest_file_b64u('MD2', @_) }

1;

=pod

=head1 NAME

Crypt::Digest::MD2 - Hash function MD2 [size: 128 bits]

=head1 SYNOPSIS

   ### Functional interface:
   use Crypt::Digest::MD2 qw( md2 md2_hex md2_b64 md2_b64u
                                md2_file md2_file_hex md2_file_b64 md2_file_b64u );

   # calculate digest from string/buffer
   $md2_raw  = md2('data string');
   $md2_hex  = md2_hex('data string');
   $md2_b64  = md2_b64('data string');
   $md2_b64u = md2_b64u('data string');
   # calculate digest from file
   $md2_raw  = md2_file('filename.dat');
   $md2_hex  = md2_file_hex('filename.dat');
   $md2_b64  = md2_file_b64('filename.dat');
   $md2_b64u = md2_file_b64u('filename.dat');
   # calculate digest from filehandle
   $md2_raw  = md2_file(*FILEHANDLE);
   $md2_hex  = md2_file_hex(*FILEHANDLE);
   $md2_b64  = md2_file_b64(*FILEHANDLE);
   $md2_b64u = md2_file_b64u(*FILEHANDLE);

   ### OO interface:
   use Crypt::Digest::MD2;

   $d = Crypt::Digest::MD2->new;
   $d->add('any data');
   $d->addfile('filename.dat');
   $d->addfile(*FILEHANDLE);
   $result_raw  = $d->digest;     # raw bytes
   $result_hex  = $d->hexdigest;  # hexadecimal form
   $result_b64  = $d->b64digest;  # Base64 form
   $result_b64u = $d->b64udigest; # Base64 URL Safe form

=head1 DESCRIPTION

Provides an interface to the MD2 digest algorithm.

=head1 EXPORT

Nothing is exported by default.

You can export selected functions:

  use Crypt::Digest::MD2 qw(md2 md2_hex md2_b64 md2_b64u
                                      md2_file md2_file_hex md2_file_b64 md2_file_b64u);

Or all of them at once:

  use Crypt::Digest::MD2 ':all';

=head1 FUNCTIONS

=head2 md2

Logically joins all arguments into a single string, and returns its MD2 digest encoded as a binary string.

 $md2_raw = md2('data string');
 #or
 $md2_raw = md2('any data', 'more data', 'even more data');

=head2 md2_hex

Logically joins all arguments into a single string, and returns its MD2 digest encoded as a hexadecimal string.

 $md2_hex = md2_hex('data string');
 #or
 $md2_hex = md2_hex('any data', 'more data', 'even more data');

=head2 md2_b64

Logically joins all arguments into a single string, and returns its MD2 digest encoded as a Base64 string, B<with> trailing '=' padding.

 $md2_b64 = md2_b64('data string');
 #or
 $md2_b64 = md2_b64('any data', 'more data', 'even more data');

=head2 md2_b64u

Logically joins all arguments into a single string, and returns its MD2 digest encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $md2_b64url = md2_b64u('data string');
 #or
 $md2_b64url = md2_b64u('any data', 'more data', 'even more data');

=head2 md2_file

Reads file (defined by filename or filehandle) content, and returns its MD2 digest encoded as a binary string.

 $md2_raw = md2_file('filename.dat');
 #or
 $md2_raw = md2_file(*FILEHANDLE);

=head2 md2_file_hex

Reads file (defined by filename or filehandle) content, and returns its MD2 digest encoded as a hexadecimal string.

 $md2_hex = md2_file_hex('filename.dat');
 #or
 $md2_hex = md2_file_hex(*FILEHANDLE);

B<BEWARE:> You have to make sure that the filehandle is in binary mode before you pass it as argument to the addfile() method.

=head2 md2_file_b64

Reads file (defined by filename or filehandle) content, and returns its MD2 digest encoded as a Base64 string, B<with> trailing '=' padding.

 $md2_b64 = md2_file_b64('filename.dat');
 #or
 $md2_b64 = md2_file_b64(*FILEHANDLE);

=head2 md2_file_b64u

Reads file (defined by filename or filehandle) content, and returns its MD2 digest encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $md2_b64url = md2_file_b64u('filename.dat');
 #or
 $md2_b64url = md2_file_b64u(*FILEHANDLE);

=head1 METHODS

The OO interface provides the same set of functions as L<Crypt::Digest>.

=head2 new

 $d = Crypt::Digest::MD2->new();

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

=head2 add_bits

 $d->add_bits($bit_string);   # e.g. $d->add_bits("111100001010");
 #or
 $d->add_bits($data, $nbits); # e.g. $d->add_bits("\xF0\xA0", 16);

=head2 hashsize

 $d->hashsize;
 #or
 Crypt::Digest::MD2->hashsize();
 #or
 Crypt::Digest::MD2::hashsize();

=head2 digest

 $result_raw = $d->digest();

=head2 hexdigest

 $result_hex = $d->hexdigest();

=head2 b64digest

 $result_b64 = $d->b64digest();

=head2 b64udigest

 $result_b64url = $d->b64udigest();

=head1 SEE ALSO

=over

=item * L<CryptX|CryptX>, L<Crypt::Digest>

=item * L<https://en.wikipedia.org/wiki/MD2_(cryptography)>

=back

=cut
