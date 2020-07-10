package Crypt::Digest::Whirlpool;

### BEWARE - GENERATED FILE, DO NOT EDIT MANUALLY!

use strict;
use warnings;
our $VERSION = '0.063';

use base qw(Crypt::Digest Exporter);
our %EXPORT_TAGS = ( all => [qw( whirlpool whirlpool_hex whirlpool_b64 whirlpool_b64u whirlpool_file whirlpool_file_hex whirlpool_file_b64 whirlpool_file_b64u )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use Crypt::Digest;

sub hashsize                { Crypt::Digest::hashsize('Whirlpool')             }
sub whirlpool             { Crypt::Digest::digest_data('Whirlpool', @_)      }
sub whirlpool_hex         { Crypt::Digest::digest_data_hex('Whirlpool', @_)  }
sub whirlpool_b64         { Crypt::Digest::digest_data_b64('Whirlpool', @_)  }
sub whirlpool_b64u        { Crypt::Digest::digest_data_b64u('Whirlpool', @_) }
sub whirlpool_file        { Crypt::Digest::digest_file('Whirlpool', @_)      }
sub whirlpool_file_hex    { Crypt::Digest::digest_file_hex('Whirlpool', @_)  }
sub whirlpool_file_b64    { Crypt::Digest::digest_file_b64('Whirlpool', @_)  }
sub whirlpool_file_b64u   { Crypt::Digest::digest_file_b64u('Whirlpool', @_) }

1;

=pod

=head1 NAME

Crypt::Digest::Whirlpool - Hash function Whirlpool [size: 512 bits]

=head1 SYNOPSIS

   ### Functional interface:
   use Crypt::Digest::Whirlpool qw( whirlpool whirlpool_hex whirlpool_b64 whirlpool_b64u
                                whirlpool_file whirlpool_file_hex whirlpool_file_b64 whirlpool_file_b64u );

   # calculate digest from string/buffer
   $whirlpool_raw  = whirlpool('data string');
   $whirlpool_hex  = whirlpool_hex('data string');
   $whirlpool_b64  = whirlpool_b64('data string');
   $whirlpool_b64u = whirlpool_b64u('data string');
   # calculate digest from file
   $whirlpool_raw  = whirlpool_file('filename.dat');
   $whirlpool_hex  = whirlpool_file_hex('filename.dat');
   $whirlpool_b64  = whirlpool_file_b64('filename.dat');
   $whirlpool_b64u = whirlpool_file_b64u('filename.dat');
   # calculate digest from filehandle
   $whirlpool_raw  = whirlpool_file(*FILEHANDLE);
   $whirlpool_hex  = whirlpool_file_hex(*FILEHANDLE);
   $whirlpool_b64  = whirlpool_file_b64(*FILEHANDLE);
   $whirlpool_b64u = whirlpool_file_b64u(*FILEHANDLE);

   ### OO interface:
   use Crypt::Digest::Whirlpool;

   $d = Crypt::Digest::Whirlpool->new;
   $d->add('any data');
   $d->addfile('filename.dat');
   $d->addfile(*FILEHANDLE);
   $result_raw  = $d->digest;     # raw bytes
   $result_hex  = $d->hexdigest;  # hexadecimal form
   $result_b64  = $d->b64digest;  # Base64 form
   $result_b64u = $d->b64udigest; # Base64 URL Safe form

=head1 DESCRIPTION

Provides an interface to the Whirlpool digest algorithm.

=head1 EXPORT

Nothing is exported by default.

You can export selected functions:

  use Crypt::Digest::Whirlpool qw(whirlpool whirlpool_hex whirlpool_b64 whirlpool_b64u
                                      whirlpool_file whirlpool_file_hex whirlpool_file_b64 whirlpool_file_b64u);

Or all of them at once:

  use Crypt::Digest::Whirlpool ':all';

=head1 FUNCTIONS

=head2 whirlpool

Logically joins all arguments into a single string, and returns its Whirlpool digest encoded as a binary string.

 $whirlpool_raw = whirlpool('data string');
 #or
 $whirlpool_raw = whirlpool('any data', 'more data', 'even more data');

=head2 whirlpool_hex

Logically joins all arguments into a single string, and returns its Whirlpool digest encoded as a hexadecimal string.

 $whirlpool_hex = whirlpool_hex('data string');
 #or
 $whirlpool_hex = whirlpool_hex('any data', 'more data', 'even more data');

=head2 whirlpool_b64

Logically joins all arguments into a single string, and returns its Whirlpool digest encoded as a Base64 string, B<with> trailing '=' padding.

 $whirlpool_b64 = whirlpool_b64('data string');
 #or
 $whirlpool_b64 = whirlpool_b64('any data', 'more data', 'even more data');

=head2 whirlpool_b64u

Logically joins all arguments into a single string, and returns its Whirlpool digest encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $whirlpool_b64url = whirlpool_b64u('data string');
 #or
 $whirlpool_b64url = whirlpool_b64u('any data', 'more data', 'even more data');

=head2 whirlpool_file

Reads file (defined by filename or filehandle) content, and returns its Whirlpool digest encoded as a binary string.

 $whirlpool_raw = whirlpool_file('filename.dat');
 #or
 $whirlpool_raw = whirlpool_file(*FILEHANDLE);

=head2 whirlpool_file_hex

Reads file (defined by filename or filehandle) content, and returns its Whirlpool digest encoded as a hexadecimal string.

 $whirlpool_hex = whirlpool_file_hex('filename.dat');
 #or
 $whirlpool_hex = whirlpool_file_hex(*FILEHANDLE);

B<BEWARE:> You have to make sure that the filehandle is in binary mode before you pass it as argument to the addfile() method.

=head2 whirlpool_file_b64

Reads file (defined by filename or filehandle) content, and returns its Whirlpool digest encoded as a Base64 string, B<with> trailing '=' padding.

 $whirlpool_b64 = whirlpool_file_b64('filename.dat');
 #or
 $whirlpool_b64 = whirlpool_file_b64(*FILEHANDLE);

=head2 whirlpool_file_b64u

Reads file (defined by filename or filehandle) content, and returns its Whirlpool digest encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $whirlpool_b64url = whirlpool_file_b64u('filename.dat');
 #or
 $whirlpool_b64url = whirlpool_file_b64u(*FILEHANDLE);

=head1 METHODS

The OO interface provides the same set of functions as L<Crypt::Digest>.

=head2 new

 $d = Crypt::Digest::Whirlpool->new();

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
 Crypt::Digest::Whirlpool->hashsize();
 #or
 Crypt::Digest::Whirlpool::hashsize();

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

=item * L<https://en.wikipedia.org/wiki/Whirlpool_(cryptography)>

=back

=cut
