package Crypt::Digest::CHAES;

### BEWARE - GENERATED FILE, DO NOT EDIT MANUALLY!

use strict;
use warnings;
our $VERSION = '0.063';

use base qw(Crypt::Digest Exporter);
our %EXPORT_TAGS = ( all => [qw( chaes chaes_hex chaes_b64 chaes_b64u chaes_file chaes_file_hex chaes_file_b64 chaes_file_b64u )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use Crypt::Digest;

sub hashsize                { Crypt::Digest::hashsize('CHAES')             }
sub chaes             { Crypt::Digest::digest_data('CHAES', @_)      }
sub chaes_hex         { Crypt::Digest::digest_data_hex('CHAES', @_)  }
sub chaes_b64         { Crypt::Digest::digest_data_b64('CHAES', @_)  }
sub chaes_b64u        { Crypt::Digest::digest_data_b64u('CHAES', @_) }
sub chaes_file        { Crypt::Digest::digest_file('CHAES', @_)      }
sub chaes_file_hex    { Crypt::Digest::digest_file_hex('CHAES', @_)  }
sub chaes_file_b64    { Crypt::Digest::digest_file_b64('CHAES', @_)  }
sub chaes_file_b64u   { Crypt::Digest::digest_file_b64u('CHAES', @_) }

1;

=pod

=head1 NAME

Crypt::Digest::CHAES - Hash function - CipherHash based on AES [size: 128 bits]

=head1 SYNOPSIS

   ### Functional interface:
   use Crypt::Digest::CHAES qw( chaes chaes_hex chaes_b64 chaes_b64u
                                chaes_file chaes_file_hex chaes_file_b64 chaes_file_b64u );

   # calculate digest from string/buffer
   $chaes_raw  = chaes('data string');
   $chaes_hex  = chaes_hex('data string');
   $chaes_b64  = chaes_b64('data string');
   $chaes_b64u = chaes_b64u('data string');
   # calculate digest from file
   $chaes_raw  = chaes_file('filename.dat');
   $chaes_hex  = chaes_file_hex('filename.dat');
   $chaes_b64  = chaes_file_b64('filename.dat');
   $chaes_b64u = chaes_file_b64u('filename.dat');
   # calculate digest from filehandle
   $chaes_raw  = chaes_file(*FILEHANDLE);
   $chaes_hex  = chaes_file_hex(*FILEHANDLE);
   $chaes_b64  = chaes_file_b64(*FILEHANDLE);
   $chaes_b64u = chaes_file_b64u(*FILEHANDLE);

   ### OO interface:
   use Crypt::Digest::CHAES;

   $d = Crypt::Digest::CHAES->new;
   $d->add('any data');
   $d->addfile('filename.dat');
   $d->addfile(*FILEHANDLE);
   $result_raw  = $d->digest;     # raw bytes
   $result_hex  = $d->hexdigest;  # hexadecimal form
   $result_b64  = $d->b64digest;  # Base64 form
   $result_b64u = $d->b64udigest; # Base64 URL Safe form

=head1 DESCRIPTION

Provides an interface to the CHAES digest algorithm.

=head1 EXPORT

Nothing is exported by default.

You can export selected functions:

  use Crypt::Digest::CHAES qw(chaes chaes_hex chaes_b64 chaes_b64u
                                      chaes_file chaes_file_hex chaes_file_b64 chaes_file_b64u);

Or all of them at once:

  use Crypt::Digest::CHAES ':all';

=head1 FUNCTIONS

=head2 chaes

Logically joins all arguments into a single string, and returns its CHAES digest encoded as a binary string.

 $chaes_raw = chaes('data string');
 #or
 $chaes_raw = chaes('any data', 'more data', 'even more data');

=head2 chaes_hex

Logically joins all arguments into a single string, and returns its CHAES digest encoded as a hexadecimal string.

 $chaes_hex = chaes_hex('data string');
 #or
 $chaes_hex = chaes_hex('any data', 'more data', 'even more data');

=head2 chaes_b64

Logically joins all arguments into a single string, and returns its CHAES digest encoded as a Base64 string, B<with> trailing '=' padding.

 $chaes_b64 = chaes_b64('data string');
 #or
 $chaes_b64 = chaes_b64('any data', 'more data', 'even more data');

=head2 chaes_b64u

Logically joins all arguments into a single string, and returns its CHAES digest encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $chaes_b64url = chaes_b64u('data string');
 #or
 $chaes_b64url = chaes_b64u('any data', 'more data', 'even more data');

=head2 chaes_file

Reads file (defined by filename or filehandle) content, and returns its CHAES digest encoded as a binary string.

 $chaes_raw = chaes_file('filename.dat');
 #or
 $chaes_raw = chaes_file(*FILEHANDLE);

=head2 chaes_file_hex

Reads file (defined by filename or filehandle) content, and returns its CHAES digest encoded as a hexadecimal string.

 $chaes_hex = chaes_file_hex('filename.dat');
 #or
 $chaes_hex = chaes_file_hex(*FILEHANDLE);

B<BEWARE:> You have to make sure that the filehandle is in binary mode before you pass it as argument to the addfile() method.

=head2 chaes_file_b64

Reads file (defined by filename or filehandle) content, and returns its CHAES digest encoded as a Base64 string, B<with> trailing '=' padding.

 $chaes_b64 = chaes_file_b64('filename.dat');
 #or
 $chaes_b64 = chaes_file_b64(*FILEHANDLE);

=head2 chaes_file_b64u

Reads file (defined by filename or filehandle) content, and returns its CHAES digest encoded as a Base64 URL Safe string (see RFC 4648 section 5).

 $chaes_b64url = chaes_file_b64u('filename.dat');
 #or
 $chaes_b64url = chaes_file_b64u(*FILEHANDLE);

=head1 METHODS

The OO interface provides the same set of functions as L<Crypt::Digest>.

=head2 new

 $d = Crypt::Digest::CHAES->new();

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
 Crypt::Digest::CHAES->hashsize();
 #or
 Crypt::Digest::CHAES::hashsize();

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

=item * L<https://en.wikipedia.org/wiki/Cryptographic_hash_function#Hash_functions_based_on_block_ciphers>

=back

=cut
