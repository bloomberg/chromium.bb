package Crypt::Digest::RIPEMD128;

### BEWARE - GENERATED FILE, DO NOT EDIT MANUALLY!

use strict;
use warnings;
our $VERSION = '0.063';

use base qw(Crypt::Digest Exporter);
our %EXPORT_TAGS = ( all => [qw( ripemd128 ripemd128_hex ripemd128_b64 ripemd128_b64u ripemd128_file ripemd128_file_hex ripemd128_file_b64 ripemd128_file_b64u )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use Crypt::Digest;

sub hashsize                { Crypt::Digest::hashsize('RIPEMD128')             }
sub ripemd128             { Crypt::Digest::digest_data('RIPEMD128', @_)      }
sub ripemd128_hex         { Crypt::Digest::digest_data_hex('RIPEMD128', @_)  }
sub ripemd128_b64         { Crypt::Digest::digest_data_b64('RIPEMD128', @_)  }
sub ripemd128_b64u        { Crypt::Digest::digest_data_b64u('RIPEMD128', @_) }
sub ripemd128_file        { Crypt::Digest::digest_file('RIPEMD128', @_)      }
sub ripemd128_file_hex    { Crypt::Digest::digest_file_hex('RIPEMD128', @_)  }
sub ripemd128_file_b64    { Crypt::Digest::digest_file_b64('RIPEMD128', @_)  }
sub ripemd128_file_b64u   { Crypt::Digest::digest_file_b64u('RIPEMD128', @_) }

1;

=pod

=head1 NAME

Crypt::Digest::RIPEMD128 - Hash function RIPEMD-128 [size: 128 bits]

=head1 SYNOPSIS

   ### Functional interface:
   use Crypt::Digest::RIPEMD128 qw( ripemd128 ripemd128_hex ripemd128_b64 ripemd128_b64u
                                ripemd128_file ripemd128_file_hex ripemd128_file_b64 ripemd128_file_b64u );

   # calculate digest from string/buffer
   $ripemd128_raw  = ripemd128('data string');
   $ripemd128_hex  = ripemd128_hex('data string');
   $ripemd128_b64  = ripemd128_b64('data string');
   $ripemd128_b64u = ripemd128_b64u('data string');
   # calculate digest from file
   $ripemd128_raw  = ripemd128_file('filename.dat');
   $ripemd128_hex  = ripemd128_file_hex('filename.dat');
   $ripemd128_b64  = ripemd128_file_b64('filename.dat');
   $ripemd128_b64u = ripemd128_file_b64u('filename.dat');
   # calculate digest from filehandle
   $ripemd128_raw  = ripemd128_file(*FILEHANDLE);
   $ripemd128_hex  = ripemd128_file_hex(*FILEHANDLE);
   $ripemd128_b64  = ripemd128_file_b64(*FILEHANDLE);
   $ripemd128_b64u = ripemd128_file_b64u(*FILEHANDLE);

   ### OO interface:
   use Crypt::Digest::RIPEMD128;

   $d = Crypt::Digest::RIPEMD128->new;
   $d->add('any data');
   $d->addfile('filename.dat');
   $d->addfile(*FILEHANDLE);
   $result_raw  = $d->digest;     # raw bytes
   $result_hex  = $d->hexdigest;  # hexadecimal form
   $result_b64  = $d->b64digest;  # Base64 form
   $result_b64u = $d->b64udigest; # Base64 URL Safe form

=head1 DESCRIPTION

Provides an interface to the RIPEMD128 digest algorithm.

=head1 EXPORT

Nothing is exported by default.

You can export selected functions:

  use Crypt::Digest::RIPEMD128 qw(ripemd128 ripemd128_hex ripemd128_b64 ripemd128_b64u
                                      ripemd128_file ripemd128_file_hex ripemd128_file_b64 ripemd128_file_b64u);

Or all of them at once:

  use Crypt::Digest::RIPEMD128 ':all';

=head1 FUNCTIONS

=head2 ripemd128

Logically joins all arguments into a single string, and returns its RIPEMD128 digest encoded as a binary string.

 $ripemd128_raw = ripemd128('data string');
 #or
 $ripemd128_raw = ripemd128('any data', 'more data', 'even more data');

=head2 ripemd128_hex

Logically joins all arguments into a single string, and returns its RIPEMD128 digest encoded as a hexadecimal string.

 $ripemd128_hex = ripemd128_hex('data string');
 #or
 $ripemd128_hex = ripemd128_hex('any data', 'more data', 'even more data');

=head2 ripemd128_b64

Logically joins all arguments into a single string, and returns its RIPEMD128 digest encoded as a Base64 string, B<with> trailing '=' padding.

 $ripemd128_b64 = ripemd128_b64('data string');
 #or
 $ripemd128_b64 = ripemd128_b64('any data', 'more data', 'even more data');

=head2 ripemd128_b64u

Logically joins all arguments into a single string, and returns its RIPEMD128 digest encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $ripemd128_b64url = ripemd128_b64u('data string');
 #or
 $ripemd128_b64url = ripemd128_b64u('any data', 'more data', 'even more data');

=head2 ripemd128_file

Reads file (defined by filename or filehandle) content, and returns its RIPEMD128 digest encoded as a binary string.

 $ripemd128_raw = ripemd128_file('filename.dat');
 #or
 $ripemd128_raw = ripemd128_file(*FILEHANDLE);

=head2 ripemd128_file_hex

Reads file (defined by filename or filehandle) content, and returns its RIPEMD128 digest encoded as a hexadecimal string.

 $ripemd128_hex = ripemd128_file_hex('filename.dat');
 #or
 $ripemd128_hex = ripemd128_file_hex(*FILEHANDLE);

B<BEWARE:> You have to make sure that the filehandle is in binary mode before you pass it as argument to the addfile() method.

=head2 ripemd128_file_b64

Reads file (defined by filename or filehandle) content, and returns its RIPEMD128 digest encoded as a Base64 string, B<with> trailing '=' padding.

 $ripemd128_b64 = ripemd128_file_b64('filename.dat');
 #or
 $ripemd128_b64 = ripemd128_file_b64(*FILEHANDLE);

=head2 ripemd128_file_b64u

Reads file (defined by filename or filehandle) content, and returns its RIPEMD128 digest encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $ripemd128_b64url = ripemd128_file_b64u('filename.dat');
 #or
 $ripemd128_b64url = ripemd128_file_b64u(*FILEHANDLE);

=head1 METHODS

The OO interface provides the same set of functions as L<Crypt::Digest>.

=head2 new

 $d = Crypt::Digest::RIPEMD128->new();

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
 Crypt::Digest::RIPEMD128->hashsize();
 #or
 Crypt::Digest::RIPEMD128::hashsize();

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

=item * L<https://en.wikipedia.org/wiki/RIPEMD>

=back

=cut
