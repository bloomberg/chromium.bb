package Crypt::Digest::SHA384;

### BEWARE - GENERATED FILE, DO NOT EDIT MANUALLY!

use strict;
use warnings;
our $VERSION = '0.063';

use base qw(Crypt::Digest Exporter);
our %EXPORT_TAGS = ( all => [qw( sha384 sha384_hex sha384_b64 sha384_b64u sha384_file sha384_file_hex sha384_file_b64 sha384_file_b64u )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use Crypt::Digest;

sub hashsize                { Crypt::Digest::hashsize('SHA384')             }
sub sha384             { Crypt::Digest::digest_data('SHA384', @_)      }
sub sha384_hex         { Crypt::Digest::digest_data_hex('SHA384', @_)  }
sub sha384_b64         { Crypt::Digest::digest_data_b64('SHA384', @_)  }
sub sha384_b64u        { Crypt::Digest::digest_data_b64u('SHA384', @_) }
sub sha384_file        { Crypt::Digest::digest_file('SHA384', @_)      }
sub sha384_file_hex    { Crypt::Digest::digest_file_hex('SHA384', @_)  }
sub sha384_file_b64    { Crypt::Digest::digest_file_b64('SHA384', @_)  }
sub sha384_file_b64u   { Crypt::Digest::digest_file_b64u('SHA384', @_) }

1;

=pod

=head1 NAME

Crypt::Digest::SHA384 - Hash function SHA-384 [size: 384 bits]

=head1 SYNOPSIS

   ### Functional interface:
   use Crypt::Digest::SHA384 qw( sha384 sha384_hex sha384_b64 sha384_b64u
                                sha384_file sha384_file_hex sha384_file_b64 sha384_file_b64u );

   # calculate digest from string/buffer
   $sha384_raw  = sha384('data string');
   $sha384_hex  = sha384_hex('data string');
   $sha384_b64  = sha384_b64('data string');
   $sha384_b64u = sha384_b64u('data string');
   # calculate digest from file
   $sha384_raw  = sha384_file('filename.dat');
   $sha384_hex  = sha384_file_hex('filename.dat');
   $sha384_b64  = sha384_file_b64('filename.dat');
   $sha384_b64u = sha384_file_b64u('filename.dat');
   # calculate digest from filehandle
   $sha384_raw  = sha384_file(*FILEHANDLE);
   $sha384_hex  = sha384_file_hex(*FILEHANDLE);
   $sha384_b64  = sha384_file_b64(*FILEHANDLE);
   $sha384_b64u = sha384_file_b64u(*FILEHANDLE);

   ### OO interface:
   use Crypt::Digest::SHA384;

   $d = Crypt::Digest::SHA384->new;
   $d->add('any data');
   $d->addfile('filename.dat');
   $d->addfile(*FILEHANDLE);
   $result_raw  = $d->digest;     # raw bytes
   $result_hex  = $d->hexdigest;  # hexadecimal form
   $result_b64  = $d->b64digest;  # Base64 form
   $result_b64u = $d->b64udigest; # Base64 URL Safe form

=head1 DESCRIPTION

Provides an interface to the SHA384 digest algorithm.

=head1 EXPORT

Nothing is exported by default.

You can export selected functions:

  use Crypt::Digest::SHA384 qw(sha384 sha384_hex sha384_b64 sha384_b64u
                                      sha384_file sha384_file_hex sha384_file_b64 sha384_file_b64u);

Or all of them at once:

  use Crypt::Digest::SHA384 ':all';

=head1 FUNCTIONS

=head2 sha384

Logically joins all arguments into a single string, and returns its SHA384 digest encoded as a binary string.

 $sha384_raw = sha384('data string');
 #or
 $sha384_raw = sha384('any data', 'more data', 'even more data');

=head2 sha384_hex

Logically joins all arguments into a single string, and returns its SHA384 digest encoded as a hexadecimal string.

 $sha384_hex = sha384_hex('data string');
 #or
 $sha384_hex = sha384_hex('any data', 'more data', 'even more data');

=head2 sha384_b64

Logically joins all arguments into a single string, and returns its SHA384 digest encoded as a Base64 string, B<with> trailing '=' padding.

 $sha384_b64 = sha384_b64('data string');
 #or
 $sha384_b64 = sha384_b64('any data', 'more data', 'even more data');

=head2 sha384_b64u

Logically joins all arguments into a single string, and returns its SHA384 digest encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $sha384_b64url = sha384_b64u('data string');
 #or
 $sha384_b64url = sha384_b64u('any data', 'more data', 'even more data');

=head2 sha384_file

Reads file (defined by filename or filehandle) content, and returns its SHA384 digest encoded as a binary string.

 $sha384_raw = sha384_file('filename.dat');
 #or
 $sha384_raw = sha384_file(*FILEHANDLE);

=head2 sha384_file_hex

Reads file (defined by filename or filehandle) content, and returns its SHA384 digest encoded as a hexadecimal string.

 $sha384_hex = sha384_file_hex('filename.dat');
 #or
 $sha384_hex = sha384_file_hex(*FILEHANDLE);

B<BEWARE:> You have to make sure that the filehandle is in binary mode before you pass it as argument to the addfile() method.

=head2 sha384_file_b64

Reads file (defined by filename or filehandle) content, and returns its SHA384 digest encoded as a Base64 string, B<with> trailing '=' padding.

 $sha384_b64 = sha384_file_b64('filename.dat');
 #or
 $sha384_b64 = sha384_file_b64(*FILEHANDLE);

=head2 sha384_file_b64u

Reads file (defined by filename or filehandle) content, and returns its SHA384 digest encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $sha384_b64url = sha384_file_b64u('filename.dat');
 #or
 $sha384_b64url = sha384_file_b64u(*FILEHANDLE);

=head1 METHODS

The OO interface provides the same set of functions as L<Crypt::Digest>.

=head2 new

 $d = Crypt::Digest::SHA384->new();

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
 Crypt::Digest::SHA384->hashsize();
 #or
 Crypt::Digest::SHA384::hashsize();

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

=item * L<https://en.wikipedia.org/wiki/SHA-2>

=back

=cut
