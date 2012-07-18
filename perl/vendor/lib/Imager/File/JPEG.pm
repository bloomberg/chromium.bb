package Imager::File::JPEG;
use strict;
use Imager;
use vars qw($VERSION @ISA);

BEGIN {
  $VERSION = "0.83";

  require XSLoader;
  XSLoader::load('Imager::File::JPEG', $VERSION);
}

Imager->register_reader
  (
   type=>'jpeg',
   single =>
   sub { 
     my ($im, $io, %hsh) = @_;

     ($im->{IMG},$im->{IPTCRAW}) = i_readjpeg_wiol( $io );

     unless ($im->{IMG}) {
       $im->_set_error(Imager->_error_as_msg);
       return;
     }
     return $im;
   },
  );

Imager->register_writer
  (
   type=>'jpeg',
   single => 
   sub { 
     my ($im, $io, %hsh) = @_;

     $im->_set_opts(\%hsh, "i_", $im);
     $im->_set_opts(\%hsh, "jpeg_", $im);
     $im->_set_opts(\%hsh, "exif_", $im);

     my $quality = $hsh{jpegquality};
     defined $quality or $quality = 75;

     if ( !i_writejpeg_wiol($im->{IMG}, $io, $quality)) {
       $im->_set_error(Imager->_error_as_msg);
       return;
     }

     return $im;
   },
  );

__END__

=head1 NAME

Imager::File::JPEG - read and write JPEG files

=head1 SYNOPSIS

  use Imager;

  my $img = Imager->new;
  $img->read(file=>"foo.jpg")
    or die $img->errstr;

  $img->write(file => "foo.jpg")
    or die $img->errstr;

=head1 DESCRIPTION

Imager's JPEG support is documented in L<Imager::Files>.

=head1 AUTHOR

Tony Cook <tonyc@cpan.org>

=head1 SEE ALSO

Imager, Imager::Files.

=cut
