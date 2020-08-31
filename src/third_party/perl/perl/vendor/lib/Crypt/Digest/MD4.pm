package Crypt::Digest::MD4;

### BEWARE - GENERATED FILE, DO NOT EDIT MANUALLY!

use strict;
use warnings;
our $VERSION = '0.063';

use base qw(Crypt::Digest Exporter);
our %EXPORT_TAGS = ( all => [qw( md4 md4_hex md4_b64 md4_b64u md4_file md4_file_hex md4_file_b64 md4_file_b64u )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use Crypt::Digest;

sub hashsize                { Crypt::Digest::hashsize('MD4')             }
sub md4             { Crypt::Digest::digest_data('MD4', @_)      }
sub md4_hex         { Crypt::Digest::digest_data_hex('MD4', @_)  }
sub md4_b64         { Crypt::Digest::digest_data_b64('MD4', @_)  }
sub md4_b64u        { Crypt::Digest::digest_data_b64u('MD4', @_) }
sub md4_file        { Crypt::Digest::digest_file('MD4', @_)      }
sub md4_file_hex    { Crypt::Digest::digest_file_hex('MD4', @_)  }
sub md4_file_b64    { Crypt::Digest::digest_file_b64('MD4', @_)  }
sub md4_file_b64u   { Crypt::Digest::digest_file_b64u('MD4', @_) }

1;

=pod

=head1 NAME

Crypt::Digest::MD4 - Hash function MD4 [size: 128 bits]

=head1 SYNOPSIS

   ### Functional interface:
   use Crypt::Digest::MD4 qw( md4 md4_hex md4_b64 md4_b64u
                                md4_file md4_file_hex md4_file_b64 md4_file_b64u );

   # calculate digest from string/buffer
   $md4_raw  = md4('data string');
   $md4_hex  = md4_hex('data string');
   $md4_b64  = md4_b64('data string');
   $md4_b64u = md4_b64u('data string');
   # calculate digest from file
   $md4_raw  = md4_file('filename.dat');
   $md4_hex  = md4_file_hex('filename.dat');
   $md4_b64  = md4_file_b64('filename.dat');
   $md4_b64u = md4_file_b64u('filename.dat');
   # calculate digest from filehandle
   $md4_raw  = md4_file(*FILEHANDLE);
   $md4_hex  = md4_file_hex(*FILEHANDLE);
   $md4_b64  = md4_file_b64(*FILEHANDLE);
   $md4_b64u = md4_file_b64u(*FILEHANDLE);

   ### OO interface:
   use Crypt::Digest::MD4;

   $d = Crypt::Digest::MD4->new;
   $d->add('any data');
   $d->addfile('filename.dat');
   $d->addfile(*FILEHANDLE);
   $result_raw  = $d->digest;     # raw bytes
   $result_hex  = $d->hexdigest;  # hexadecimal form
   $result_b64  = $d->b64digest;  # Base64 form
   $result_b64u = $d->b64udigest; # Base64 URL Safe form

=head1 DESCRIPTION

Provides an interface to the MD4 digest algorithm.

=head1 EXPORT

Nothing is exported by default.

You can export selected functions:

  use Crypt::Digest::MD4 qw(md4 md4_hex md4_b64 md4_b64u
                                      md4_file md4_file_hex md4_file_b64 md4_file_b64u);

Or all of them at once:

  use Crypt::Digest::MD4 ':all';

=head1 FUNCTIONS

=head2 md4

Logically joins all arguments into a single string, and returns its MD4 digest encoded as a binary string.

 $md4_raw = md4('data string');
 #or
 $md4_raw = md4('any data', 'more data', 'even more data');

=head2 md4_hex

Logically joins all arguments into a single string, and returns its MD4 digest encoded as a hexadecimal string.

 $md4_hex = md4_hex('data string');
 #or
 $md4_hex = md4_hex('any data', 'more data', 'even more data');

=head2 md4_b64

Logically joins all arguments into a single string, and returns its MD4 digest encoded as a Base64 string, B<with> trailing '=' padding.

 $md4_b64 = md4_b64('data string');
 #or
 $md4_b64 = md4_b64('any data', 'more data', 'even more data');

=head2 md4_b64u

Logically joins all arguments into a single string, and returns its MD4 digest encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $md4_b64url = md4_b64u('data string');
 #or
 $md4_b64url = md4_b64u('any data', 'more data', 'even more data');

=head2 md4_file

Reads file (defined by filename or filehandle) content, and returns its MD4 digest encoded as a binary string.

 $md4_raw = md4_file('filename.dat');
 #or
 $md4_raw = md4_file(*FILEHANDLE);

=head2 md4_file_hex

Reads file (defined by filename or filehandle) content, and returns its MD4 digest encoded as a hexadecimal string.

 $md4_hex = md4_file_hex('filename.dat');
 #or
 $md4_hex = md4_file_hex(*FILEHANDLE);

B<BEWARE:> You have to make sure that the filehandle is in binary mode before you pass it as argument to the addfile() method.

=head2 md4_file_b64

Reads file (defined by filename or filehandle) content, and returns its MD4 digest encoded as a Base64 string, B<with> trailing '=' padding.

 $md4_b64 = md4_file_b64('filename.dat');
 #or
 $md4_b64 = md4_file_b64(*FILEHANDLE);

=head2 md4_file_b64u

Reads file (defined by filename or filehandle) content, and returns its MD4 digest encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $md4_b64url = md4_file_b64u('filename.dat');
 #or
 $md4_b64url = md4_file_b64u(*FILEHANDLE);

=head1 METHODS

The OO interface provides the same set of functions as L<Crypt::Digest>.

=head2 new

 $d = Crypt::Digest::MD4->new();

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
 Crypt::Digest::MD4->hashsize();
 #or
 Crypt::Digest::MD4::hashsize();

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

=item * L<https://en.wikipedia.org/wiki/MD4>

=back

=cut
