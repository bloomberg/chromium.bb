package Crypt::Checksum::Adler32;

use strict;
use warnings;
our $VERSION = '0.063';

use base qw(Crypt::Checksum Exporter);
our %EXPORT_TAGS = ( all => [qw( adler32_data adler32_data_hex adler32_data_int adler32_file adler32_file_hex adler32_file_int )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use CryptX;

sub adler32_file     { local $SIG{__DIE__} = \&CryptX::_croak; Crypt::Checksum::Adler32->new->addfile(@_)->digest    }
sub adler32_file_hex { local $SIG{__DIE__} = \&CryptX::_croak; Crypt::Checksum::Adler32->new->addfile(@_)->hexdigest }
sub adler32_file_int { local $SIG{__DIE__} = \&CryptX::_croak; Crypt::Checksum::Adler32->new->addfile(@_)->intdigest }

1;

=pod

=head1 NAME

Crypt::Checksum::Adler32 - Compute Adler32 checksum

=head1 SYNOPSIS

   ### Functional interface:
   use Crypt::Checksum::Adler32 ':all';

   # calculate Adler32 checksum from string/buffer
   $checksum_raw  = adler32_data($data);
   $checksum_hex  = adler32_data_hex($data);
   $checksum_int  = adler32_data_int($data);
   # calculate Adler32 checksum from file
   $checksum_raw  = adler32_file('filename.dat');
   $checksum_hex  = adler32_file_hex('filename.dat');
   $checksum_int  = adler32_file_int('filename.dat');
   # calculate Adler32 checksum from filehandle
   $checksum_raw  = adler32_file(*FILEHANDLE);
   $checksum_hex  = adler32_file_hex(*FILEHANDLE);
   $checksum_int  = adler32_file_int(*FILEHANDLE);

   ### OO interface:
   use Crypt::Checksum::Adler32;

   $d = Crypt::Checksum::Adler32->new;
   $d->add('any data');
   $d->add('another data');
   $d->addfile('filename.dat');
   $d->addfile(*FILEHANDLE);
   $checksum_raw  = $d->digest;     # raw 4 bytes
   $checksum_hex  = $d->hexdigest;  # hexadecimal form
   $checksum_int  = $d->intdigest;  # 32bit unsigned integer

=head1 DESCRIPTION

Calculating Adler32 checksums.

I<Updated: v0.057>

=head1 EXPORT

Nothing is exported by default.

You can export selected functions:

 use Crypt::Checksum::Adler32 qw(adler32_data adler32_data_hex adler32_data_int adler32_file adler32_file_hex adler32_file_int);

Or all of them at once:

 use Crypt::Checksum::Adler32 ':all';

=head1 FUNCTIONS

=head2 adler32_data

Returns checksum as raw octects.

 $checksum_raw = adler32_data('data string');
 #or
 $checksum_raw = adler32_data('any data', 'more data', 'even more data');

=head2 adler32_data_hex

Returns checksum as a hexadecimal string.

 $checksum_hex = adler32_data_hex('data string');
 #or
 $checksum_hex = adler32_data_hex('any data', 'more data', 'even more data');

=head2 adler32_data_int

Returns checksum as unsigned 32bit integer.

 $checksum_int = adler32_data_int('data string');
 #or
 $checksum_int = adler32_data_int('any data', 'more data', 'even more data');

=head2 adler32_file

Returns checksum as raw octects.

 $checksum_raw = adler32_file('filename.dat');
 #or
 $checksum_raw = adler32_file(*FILEHANDLE);

=head2 adler32_file_hex

Returns checksum as a hexadecimal string.

 $checksum_hex = adler32_file_hex('filename.dat');
 #or
 $checksum_hex = adler32_file_hex(*FILEHANDLE);

=head2 adler32_file_int

Returns checksum as unsigned 32bit integer.

 $checksum_int = adler32_file_int('filename.dat');
 #or
 $checksum_int = adler32_file_int(*FILEHANDLE);

=head1 METHODS

=head2 new

Constructor, returns a reference to the checksum object.

 $d = Crypt::Checksum::Adler32->new;

=head2 clone

Creates a copy of the checksum object state and returns a reference to the copy.

 $d->clone();

=head2 reset

Reinitialize the checksum object state and returns a reference to the checksum object.

 $d->reset();

=head2 add

All arguments are appended to the message we calculate checksum for.
The return value is the checksum object itself.

 $d->add('any data');
 #or
 $d->add('any data', 'more data', 'even more data');

=head2 addfile

The content of the file (or filehandle) is appended to the message we calculate checksum for.
The return value is the checksum object itself.

 $d->addfile('filename.dat');
 #or
 $d->addfile(*FILEHANDLE);

B<BEWARE:> You have to make sure that the filehandle is in binary mode before you pass it as argument to the addfile() method.

=head2 digest

Returns the binary checksum (raw bytes).

 $result_raw = $d->digest();

=head2 hexdigest

Returns the checksum encoded as a hexadecimal string.

 $result_hex = $d->hexdigest();

=head2 intdigest

Returns the checksum encoded as unsigned 32bit integer.

 $result_int = $d->intdigest();

=head1 SEE ALSO

=over

=item * L<CryptX|CryptX>

=item * L<https://en.wikipedia.org/wiki/Adler-32>

=back

=cut
