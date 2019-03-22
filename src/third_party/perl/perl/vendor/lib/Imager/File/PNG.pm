package Imager::File::PNG;
use strict;
use Imager;
use vars qw($VERSION @ISA);

BEGIN {
  $VERSION = "0.84";

  require XSLoader;
  XSLoader::load('Imager::File::PNG', $VERSION);
}

Imager->register_reader
  (
   type=>'png',
   single => 
   sub { 
     my ($im, $io, %hsh) = @_;
     $im->{IMG} = i_readpng_wiol($io);

     unless ($im->{IMG}) {
       $im->_set_error(Imager->_error_as_msg);
       return;
     }
     return $im;
   },
  );

Imager->register_writer
  (
   type=>'png',
   single => 
   sub { 
     my ($im, $io, %hsh) = @_;

     $im->_set_opts(\%hsh, "i_", $im);
     $im->_set_opts(\%hsh, "png_", $im);

     unless (i_writepng_wiol($im->{IMG}, $io)) {
       $im->_set_error(Imager->_error_as_msg);
       return;
     }
     return $im;
   },
  );

__END__

=head1 NAME

Imager::File::PNG - read and write PNG files

=head1 SYNOPSIS

  use Imager;

  my $img = Imager->new;
  $img->read(file=>"foo.png")
    or die $img->errstr;

  $img->write(file => "foo.png")
    or die $img->errstr;

=head1 DESCRIPTION

Imager's PNG support is documented in L<Imager::Files>.

=head1 AUTHOR

Tony Cook <tonyc@cpan.org>

=head1 SEE ALSO

Imager, Imager::Files.

=cut
