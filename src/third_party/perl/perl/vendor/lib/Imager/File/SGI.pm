package Imager::File::SGI;
use strict;
use Imager;
use vars qw($VERSION @ISA);

BEGIN {
  $VERSION = "0.03";
  
  require XSLoader;
  XSLoader::load('Imager::File::SGI', $VERSION);
}

Imager->register_reader
  (
   type=>'sgi',
   single => 
   sub { 
     my ($im, $io, %hsh) = @_;
     $im->{IMG} = i_readsgi_wiol($io, $hsh{page} || 0);

     unless ($im->{IMG}) {
       $im->_set_error(Imager->_error_as_msg);
       return;
     }
     return $im;
   },
  );

Imager->register_writer
  (
   type=>'sgi',
   single => 
   sub { 
     my ($im, $io, %hsh) = @_;

     $im->_set_opts(\%hsh, "i_", $im);
     $im->_set_opts(\%hsh, "sgi_", $im);

     unless (i_writesgi_wiol($io, $im->{IMG})) {
       $im->_set_error(Imager->_error_as_msg);
       return;
     }
     return $im;
   },
  );

__END__

=head1 NAME

Imager::File::ICO - read MS Icon files

=head1 SYNOPSIS

  use Imager;

  my $img = Imager->new;
  $img->read(file=>"foo.ico")
    or die $img->errstr;

  my @imgs = Imager->read_multi(file => "foo.ico")
    or die Imager->errstr;

  $img->write(file => "foo.ico")
    or die $img->errstr;

  Imager->write_multi({ file => "foo.ico" }, @imgs)
    or die Imager->errstr;

=head1 DESCRIPTION

Imager's MS Icon support is documented in L<Imager::Files>.

=head1 AUTHOR

Tony Cook <tonyc@cpan.org>

=head1 SEE ALSO

Imager, Imager::Files.

=cut
