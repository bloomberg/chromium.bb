package Crypt::Digest::SHAKE;

use strict;
use warnings;
our $VERSION = '0.063';

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use CryptX;

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

Crypt::Digest::SHAKE - Hash functions SHAKE128, SHAKE256 from SHA3 family

=head1 SYNOPSIS

   use Crypt::Digest::SHAKE

   $d = Crypt::Digest::SHAKE->new(128);
   $d->add('any data');
   $d->addfile('filename.dat');
   $d->addfile(*FILEHANDLE);
   $part1 = $d->done(100); # 100 raw bytes
   $part2 = $d->done(100); # another 100 raw bytes
   #...

=head1 DESCRIPTION

Provides an interface to the SHA3's sponge function SHAKE.

=head1 METHODS

=head2 new

 $d = Crypt::Digest::SHA3-SHAKE->new($num);
 # $num ... 128 or 256

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

=head2 done

 $result_raw = $d->done($len);
 # can be called multiple times

=head1 SEE ALSO

=over

=item * L<CryptX|CryptX>, L<Crypt::Digest|Crypt::Digest>

=item * L<http://en.wikipedia.org/wiki/SHA-3|http://en.wikipedia.org/wiki/SHA-3>

=back

=cut
