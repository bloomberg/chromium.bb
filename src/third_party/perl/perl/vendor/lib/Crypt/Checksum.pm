package Crypt::Checksum;

use strict;
use warnings;
our $VERSION = '0.063';

require Exporter; our @ISA = qw(Exporter); ### use Exporter 'import';
our %EXPORT_TAGS = ( all => [qw/ adler32_data adler32_data_hex adler32_data_int adler32_file adler32_file_hex adler32_file_int
                                 crc32_data crc32_data_hex crc32_data_int crc32_file crc32_file_hex crc32_file_int /] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
$Carp::Internal{(__PACKAGE__)}++;

# obsolete since v0.057, only for backwards compatibility
use Crypt::Checksum::CRC32;
use Crypt::Checksum::Adler32;
sub adler32_data        { goto \&Crypt::Checksum::Adler32::adler32_data     }
sub adler32_data_hex    { goto \&Crypt::Checksum::Adler32::adler32_data_hex }
sub adler32_data_int    { goto \&Crypt::Checksum::Adler32::adler32_data_int }
sub adler32_file        { goto \&Crypt::Checksum::Adler32::adler32_file     }
sub adler32_file_hex    { goto \&Crypt::Checksum::Adler32::adler32_file_hex }
sub adler32_file_int    { goto \&Crypt::Checksum::Adler32::adler32_file_int }
sub crc32_data          { goto \&Crypt::Checksum::CRC32::crc32_data     }
sub crc32_data_hex      { goto \&Crypt::Checksum::CRC32::crc32_data_hex }
sub crc32_data_int      { goto \&Crypt::Checksum::CRC32::crc32_data_int }
sub crc32_file          { goto \&Crypt::Checksum::CRC32::crc32_file     }
sub crc32_file_hex      { goto \&Crypt::Checksum::CRC32::crc32_file_hex }
sub crc32_file_int      { goto \&Crypt::Checksum::CRC32::crc32_file_int }

sub addfile {
  my ($self, $file) = @_;

  my $handle;
  if (ref(\$file) eq 'SCALAR') {        #filename
    open($handle, "<", $file) || croak "FATAL: cannot open '$file': $!";
    binmode($handle);
  }
  else {                                #handle
    $handle = $file
  }
  croak "FATAL: invalid handle" unless defined $handle;

  my $n;
  my $buf = "";
  while (($n = read($handle, $buf, 32*1024))) {
    $self->add($buf)
  }
  croak "FATAL: read failed: $!" unless defined $n;

  return $self;
}

sub CLONE_SKIP { 1 } # prevent cloning

1;

=pod

=head1 NAME

Crypt::Checksum - [internal only]

=head1 DESCRIPTION

You are probably looking for L<Crypt::Checksum::CRC32> or L<Crypt::Checksum::Adler32>.

=cut
