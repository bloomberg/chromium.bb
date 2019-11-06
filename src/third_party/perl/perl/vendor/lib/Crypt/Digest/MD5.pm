package Crypt::Digest::MD5;

### BEWARE - GENERATED FILE, DO NOT EDIT MANUALLY!

use strict;
use warnings;
our $VERSION = '0.063';

use base qw(Crypt::Digest Exporter);
our %EXPORT_TAGS = ( all => [qw( md5 md5_hex md5_b64 md5_b64u md5_file md5_file_hex md5_file_b64 md5_file_b64u )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use Crypt::Digest;

sub hashsize                { Crypt::Digest::hashsize('MD5')             }
sub md5             { Crypt::Digest::digest_data('MD5', @_)      }
sub md5_hex         { Crypt::Digest::digest_data_hex('MD5', @_)  }
sub md5_b64         { Crypt::Digest::digest_data_b64('MD5', @_)  }
sub md5_b64u        { Crypt::Digest::digest_data_b64u('MD5', @_) }
sub md5_file        { Crypt::Digest::digest_file('MD5', @_)      }
sub md5_file_hex    { Crypt::Digest::digest_file_hex('MD5', @_)  }
sub md5_file_b64    { Crypt::Digest::digest_file_b64('MD5', @_)  }
sub md5_file_b64u   { Crypt::Digest::digest_file_b64u('MD5', @_) }

1;

=pod

=head1 NAME

Crypt::Digest::MD5 - Hash function MD5 [size: 128 bits]

=head1 SYNOPSIS

   ### Functional interface:
   use Crypt::Digest::MD5 qw( md5 md5_hex md5_b64 md5_b64u
                                md5_file md5_file_hex md5_file_b64 md5_file_b64u );

   # calculate digest from string/buffer
   $md5_raw  = md5('data string');
   $md5_hex  = md5_hex('data string');
   $md5_b64  = md5_b64('data string');
   $md5_b64u = md5_b64u('data string');
   # calculate digest from file
   $md5_raw  = md5_file('filename.dat');
   $md5_hex  = md5_file_hex('filename.dat');
   $md5_b64  = md5_file_b64('filename.dat');
   $md5_b64u = md5_file_b64u('filename.dat');
   # calculate digest from filehandle
   $md5_raw  = md5_file(*FILEHANDLE);
   $md5_hex  = md5_file_hex(*FILEHANDLE);
   $md5_b64  = md5_file_b64(*FILEHANDLE);
   $md5_b64u = md5_file_b64u(*FILEHANDLE);

   ### OO interface:
   use Crypt::Digest::MD5;

   $d = Crypt::Digest::MD5->new;
   $d->add('any data');
   $d->addfile('filename.dat');
   $d->addfile(*FILEHANDLE);
   $result_raw  = $d->digest;     # raw bytes
   $result_hex  = $d->hexdigest;  # hexadecimal form
   $result_b64  = $d->b64digest;  # Base64 form
   $result_b64u = $d->b64udigest; # Base64 URL Safe form

=head1 DESCRIPTION

Provides an interface to the MD5 digest algorithm.

=head1 EXPORT

Nothing is exported by default.

You can export selected functions:

  use Crypt::Digest::MD5 qw(md5 md5_hex md5_b64 md5_b64u
                                      md5_file md5_file_hex md5_file_b64 md5_file_b64u);

Or all of them at once:

  use Crypt::Digest::MD5 ':all';

=head1 FUNCTIONS

=head2 md5

Logically joins all arguments into a single string, and returns its MD5 digest encoded as a binary string.

 $md5_raw = md5('data string');
 #or
 $md5_raw = md5('any data', 'more data', 'even more data');

=head2 md5_hex

Logically joins all arguments into a single string, and returns its MD5 digest encoded as a hexadecimal string.

 $md5_hex = md5_hex('data string');
 #or
 $md5_hex = md5_hex('any data', 'more data', 'even more data');

=head2 md5_b64

Logically joins all arguments into a single string, and returns its MD5 digest encoded as a Base64 string, B<with> trailing '=' padding.

 $md5_b64 = md5_b64('data string');
 #or
 $md5_b64 = md5_b64('any data', 'more data', 'even more data');

=head2 md5_b64u

Logically joins all arguments into a single string, and returns its MD5 digest encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $md5_b64url = md5_b64u('data string');
 #or
 $md5_b64url = md5_b64u('any data', 'more data', 'even more data');

=head2 md5_file

Reads file (defined by filename or filehandle) content, and returns its MD5 digest encoded as a binary string.

 $md5_raw = md5_file('filename.dat');
 #or
 $md5_raw = md5_file(*FILEHANDLE);

=head2 md5_file_hex

Reads file (defined by filename or filehandle) content, and returns its MD5 digest encoded as a hexadecimal string.

 $md5_hex = md5_file_hex('filename.dat');
 #or
 $md5_hex = md5_file_hex(*FILEHANDLE);

B<BEWARE:> You have to make sure that the filehandle is in binary mode before you pass it as argument to the addfile() method.

=head2 md5_file_b64

Reads file (defined by filename or filehandle) content, and returns its MD5 digest encoded as a Base64 string, B<with> trailing '=' padding.

 $md5_b64 = md5_file_b64('filename.dat');
 #or
 $md5_b64 = md5_file_b64(*FILEHANDLE);

=head2 md5_file_b64u

Reads file (defined by filename or filehandle) content, and returns its MD5 digest encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $md5_b64url = md5_file_b64u('filename.dat');
 #or
 $md5_b64url = md5_file_b64u(*FILEHANDLE);

=head1 METHODS

The OO interface provides the same set of functions as L<Crypt::Digest>.

=head2 new

 $d = Crypt::Digest::MD5->new();

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
 Crypt::Digest::MD5->hashsize();
 #or
 Crypt::Digest::MD5::hashsize();

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

=item * L<https://en.wikipedia.org/wiki/MD5>

=back

=cut
