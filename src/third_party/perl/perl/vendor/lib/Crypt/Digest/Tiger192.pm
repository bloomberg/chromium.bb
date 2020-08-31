package Crypt::Digest::Tiger192;

### BEWARE - GENERATED FILE, DO NOT EDIT MANUALLY!

use strict;
use warnings;
our $VERSION = '0.063';

use base qw(Crypt::Digest Exporter);
our %EXPORT_TAGS = ( all => [qw( tiger192 tiger192_hex tiger192_b64 tiger192_b64u tiger192_file tiger192_file_hex tiger192_file_b64 tiger192_file_b64u )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use Crypt::Digest;

sub hashsize                { Crypt::Digest::hashsize('Tiger192')             }
sub tiger192             { Crypt::Digest::digest_data('Tiger192', @_)      }
sub tiger192_hex         { Crypt::Digest::digest_data_hex('Tiger192', @_)  }
sub tiger192_b64         { Crypt::Digest::digest_data_b64('Tiger192', @_)  }
sub tiger192_b64u        { Crypt::Digest::digest_data_b64u('Tiger192', @_) }
sub tiger192_file        { Crypt::Digest::digest_file('Tiger192', @_)      }
sub tiger192_file_hex    { Crypt::Digest::digest_file_hex('Tiger192', @_)  }
sub tiger192_file_b64    { Crypt::Digest::digest_file_b64('Tiger192', @_)  }
sub tiger192_file_b64u   { Crypt::Digest::digest_file_b64u('Tiger192', @_) }

1;

=pod

=head1 NAME

Crypt::Digest::Tiger192 - Hash function Tiger-192 [size: 192 bits]

=head1 SYNOPSIS

   ### Functional interface:
   use Crypt::Digest::Tiger192 qw( tiger192 tiger192_hex tiger192_b64 tiger192_b64u
                                tiger192_file tiger192_file_hex tiger192_file_b64 tiger192_file_b64u );

   # calculate digest from string/buffer
   $tiger192_raw  = tiger192('data string');
   $tiger192_hex  = tiger192_hex('data string');
   $tiger192_b64  = tiger192_b64('data string');
   $tiger192_b64u = tiger192_b64u('data string');
   # calculate digest from file
   $tiger192_raw  = tiger192_file('filename.dat');
   $tiger192_hex  = tiger192_file_hex('filename.dat');
   $tiger192_b64  = tiger192_file_b64('filename.dat');
   $tiger192_b64u = tiger192_file_b64u('filename.dat');
   # calculate digest from filehandle
   $tiger192_raw  = tiger192_file(*FILEHANDLE);
   $tiger192_hex  = tiger192_file_hex(*FILEHANDLE);
   $tiger192_b64  = tiger192_file_b64(*FILEHANDLE);
   $tiger192_b64u = tiger192_file_b64u(*FILEHANDLE);

   ### OO interface:
   use Crypt::Digest::Tiger192;

   $d = Crypt::Digest::Tiger192->new;
   $d->add('any data');
   $d->addfile('filename.dat');
   $d->addfile(*FILEHANDLE);
   $result_raw  = $d->digest;     # raw bytes
   $result_hex  = $d->hexdigest;  # hexadecimal form
   $result_b64  = $d->b64digest;  # Base64 form
   $result_b64u = $d->b64udigest; # Base64 URL Safe form

=head1 DESCRIPTION

Provides an interface to the Tiger192 digest algorithm.

=head1 EXPORT

Nothing is exported by default.

You can export selected functions:

  use Crypt::Digest::Tiger192 qw(tiger192 tiger192_hex tiger192_b64 tiger192_b64u
                                      tiger192_file tiger192_file_hex tiger192_file_b64 tiger192_file_b64u);

Or all of them at once:

  use Crypt::Digest::Tiger192 ':all';

=head1 FUNCTIONS

=head2 tiger192

Logically joins all arguments into a single string, and returns its Tiger192 digest encoded as a binary string.

 $tiger192_raw = tiger192('data string');
 #or
 $tiger192_raw = tiger192('any data', 'more data', 'even more data');

=head2 tiger192_hex

Logically joins all arguments into a single string, and returns its Tiger192 digest encoded as a hexadecimal string.

 $tiger192_hex = tiger192_hex('data string');
 #or
 $tiger192_hex = tiger192_hex('any data', 'more data', 'even more data');

=head2 tiger192_b64

Logically joins all arguments into a single string, and returns its Tiger192 digest encoded as a Base64 string, B<with> trailing '=' padding.

 $tiger192_b64 = tiger192_b64('data string');
 #or
 $tiger192_b64 = tiger192_b64('any data', 'more data', 'even more data');

=head2 tiger192_b64u

Logically joins all arguments into a single string, and returns its Tiger192 digest encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $tiger192_b64url = tiger192_b64u('data string');
 #or
 $tiger192_b64url = tiger192_b64u('any data', 'more data', 'even more data');

=head2 tiger192_file

Reads file (defined by filename or filehandle) content, and returns its Tiger192 digest encoded as a binary string.

 $tiger192_raw = tiger192_file('filename.dat');
 #or
 $tiger192_raw = tiger192_file(*FILEHANDLE);

=head2 tiger192_file_hex

Reads file (defined by filename or filehandle) content, and returns its Tiger192 digest encoded as a hexadecimal string.

 $tiger192_hex = tiger192_file_hex('filename.dat');
 #or
 $tiger192_hex = tiger192_file_hex(*FILEHANDLE);

B<BEWARE:> You have to make sure that the filehandle is in binary mode before you pass it as argument to the addfile() method.

=head2 tiger192_file_b64

Reads file (defined by filename or filehandle) content, and returns its Tiger192 digest encoded as a Base64 string, B<with> trailing '=' padding.

 $tiger192_b64 = tiger192_file_b64('filename.dat');
 #or
 $tiger192_b64 = tiger192_file_b64(*FILEHANDLE);

=head2 tiger192_file_b64u

Reads file (defined by filename or filehandle) content, and returns its Tiger192 digest encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $tiger192_b64url = tiger192_file_b64u('filename.dat');
 #or
 $tiger192_b64url = tiger192_file_b64u(*FILEHANDLE);

=head1 METHODS

The OO interface provides the same set of functions as L<Crypt::Digest>.

=head2 new

 $d = Crypt::Digest::Tiger192->new();

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
 Crypt::Digest::Tiger192->hashsize();
 #or
 Crypt::Digest::Tiger192::hashsize();

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

=item * L<https://en.wikipedia.org/wiki/Tiger_(cryptography)>

=back

=cut
