package Crypt::Digest::RIPEMD256;

### BEWARE - GENERATED FILE, DO NOT EDIT MANUALLY!

use strict;
use warnings;
our $VERSION = '0.063';

use base qw(Crypt::Digest Exporter);
our %EXPORT_TAGS = ( all => [qw( ripemd256 ripemd256_hex ripemd256_b64 ripemd256_b64u ripemd256_file ripemd256_file_hex ripemd256_file_b64 ripemd256_file_b64u )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use Crypt::Digest;

sub hashsize                { Crypt::Digest::hashsize('RIPEMD256')             }
sub ripemd256             { Crypt::Digest::digest_data('RIPEMD256', @_)      }
sub ripemd256_hex         { Crypt::Digest::digest_data_hex('RIPEMD256', @_)  }
sub ripemd256_b64         { Crypt::Digest::digest_data_b64('RIPEMD256', @_)  }
sub ripemd256_b64u        { Crypt::Digest::digest_data_b64u('RIPEMD256', @_) }
sub ripemd256_file        { Crypt::Digest::digest_file('RIPEMD256', @_)      }
sub ripemd256_file_hex    { Crypt::Digest::digest_file_hex('RIPEMD256', @_)  }
sub ripemd256_file_b64    { Crypt::Digest::digest_file_b64('RIPEMD256', @_)  }
sub ripemd256_file_b64u   { Crypt::Digest::digest_file_b64u('RIPEMD256', @_) }

1;

=pod

=head1 NAME

Crypt::Digest::RIPEMD256 - Hash function RIPEMD-256 [size: 256 bits]

=head1 SYNOPSIS

   ### Functional interface:
   use Crypt::Digest::RIPEMD256 qw( ripemd256 ripemd256_hex ripemd256_b64 ripemd256_b64u
                                ripemd256_file ripemd256_file_hex ripemd256_file_b64 ripemd256_file_b64u );

   # calculate digest from string/buffer
   $ripemd256_raw  = ripemd256('data string');
   $ripemd256_hex  = ripemd256_hex('data string');
   $ripemd256_b64  = ripemd256_b64('data string');
   $ripemd256_b64u = ripemd256_b64u('data string');
   # calculate digest from file
   $ripemd256_raw  = ripemd256_file('filename.dat');
   $ripemd256_hex  = ripemd256_file_hex('filename.dat');
   $ripemd256_b64  = ripemd256_file_b64('filename.dat');
   $ripemd256_b64u = ripemd256_file_b64u('filename.dat');
   # calculate digest from filehandle
   $ripemd256_raw  = ripemd256_file(*FILEHANDLE);
   $ripemd256_hex  = ripemd256_file_hex(*FILEHANDLE);
   $ripemd256_b64  = ripemd256_file_b64(*FILEHANDLE);
   $ripemd256_b64u = ripemd256_file_b64u(*FILEHANDLE);

   ### OO interface:
   use Crypt::Digest::RIPEMD256;

   $d = Crypt::Digest::RIPEMD256->new;
   $d->add('any data');
   $d->addfile('filename.dat');
   $d->addfile(*FILEHANDLE);
   $result_raw  = $d->digest;     # raw bytes
   $result_hex  = $d->hexdigest;  # hexadecimal form
   $result_b64  = $d->b64digest;  # Base64 form
   $result_b64u = $d->b64udigest; # Base64 URL Safe form

=head1 DESCRIPTION

Provides an interface to the RIPEMD256 digest algorithm.

=head1 EXPORT

Nothing is exported by default.

You can export selected functions:

  use Crypt::Digest::RIPEMD256 qw(ripemd256 ripemd256_hex ripemd256_b64 ripemd256_b64u
                                      ripemd256_file ripemd256_file_hex ripemd256_file_b64 ripemd256_file_b64u);

Or all of them at once:

  use Crypt::Digest::RIPEMD256 ':all';

=head1 FUNCTIONS

=head2 ripemd256

Logically joins all arguments into a single string, and returns its RIPEMD256 digest encoded as a binary string.

 $ripemd256_raw = ripemd256('data string');
 #or
 $ripemd256_raw = ripemd256('any data', 'more data', 'even more data');

=head2 ripemd256_hex

Logically joins all arguments into a single string, and returns its RIPEMD256 digest encoded as a hexadecimal string.

 $ripemd256_hex = ripemd256_hex('data string');
 #or
 $ripemd256_hex = ripemd256_hex('any data', 'more data', 'even more data');

=head2 ripemd256_b64

Logically joins all arguments into a single string, and returns its RIPEMD256 digest encoded as a Base64 string, B<with> trailing '=' padding.

 $ripemd256_b64 = ripemd256_b64('data string');
 #or
 $ripemd256_b64 = ripemd256_b64('any data', 'more data', 'even more data');

=head2 ripemd256_b64u

Logically joins all arguments into a single string, and returns its RIPEMD256 digest encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $ripemd256_b64url = ripemd256_b64u('data string');
 #or
 $ripemd256_b64url = ripemd256_b64u('any data', 'more data', 'even more data');

=head2 ripemd256_file

Reads file (defined by filename or filehandle) content, and returns its RIPEMD256 digest encoded as a binary string.

 $ripemd256_raw = ripemd256_file('filename.dat');
 #or
 $ripemd256_raw = ripemd256_file(*FILEHANDLE);

=head2 ripemd256_file_hex

Reads file (defined by filename or filehandle) content, and returns its RIPEMD256 digest encoded as a hexadecimal string.

 $ripemd256_hex = ripemd256_file_hex('filename.dat');
 #or
 $ripemd256_hex = ripemd256_file_hex(*FILEHANDLE);

B<BEWARE:> You have to make sure that the filehandle is in binary mode before you pass it as argument to the addfile() method.

=head2 ripemd256_file_b64

Reads file (defined by filename or filehandle) content, and returns its RIPEMD256 digest encoded as a Base64 string, B<with> trailing '=' padding.

 $ripemd256_b64 = ripemd256_file_b64('filename.dat');
 #or
 $ripemd256_b64 = ripemd256_file_b64(*FILEHANDLE);

=head2 ripemd256_file_b64u

Reads file (defined by filename or filehandle) content, and returns its RIPEMD256 digest encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $ripemd256_b64url = ripemd256_file_b64u('filename.dat');
 #or
 $ripemd256_b64url = ripemd256_file_b64u(*FILEHANDLE);

=head1 METHODS

The OO interface provides the same set of functions as L<Crypt::Digest>.

=head2 new

 $d = Crypt::Digest::RIPEMD256->new();

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
 Crypt::Digest::RIPEMD256->hashsize();
 #or
 Crypt::Digest::RIPEMD256::hashsize();

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
