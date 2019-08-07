package Imager::File::GIF;
use strict;
use Imager;
use vars qw($VERSION @ISA);

BEGIN {
  $VERSION = "0.83";

  require XSLoader;
  XSLoader::load('Imager::File::GIF', $VERSION);
}

Imager->register_reader
  (
   type=>'gif',
   single => 
   sub { 
     my ($im, $io, %hsh) = @_;

     if ($hsh{gif_consolidate}) {
       if ($hsh{colors}) {
	 my $colors;
	 ($im->{IMG}, $colors) =i_readgif_wiol( $io );
	 if ($colors) {
	   ${ $hsh{colors} } = [ map { NC(@$_) } @$colors ];
	 }
       }
       else {
	 $im->{IMG} =i_readgif_wiol( $io );
       }
     }
     else {
       my $page = $hsh{page};
       defined $page or $page = 0;
       $im->{IMG} = i_readgif_single_wiol($io, $page);

       unless ($im->{IMG}) {
	 $im->_set_error(Imager->_error_as_msg);
	 return;
       }
       if ($hsh{colors}) {
	 ${ $hsh{colors} } = [ $im->getcolors ];
       }
       return $im;
     }
   },
   multiple =>
   sub {
     my ($io, %hsh) = @_;

     my @imgs = i_readgif_multi_wiol($io);
     unless (@imgs) {
       Imager->_set_error(Imager->_error_as_msg);
       return;
     }

     return map bless({ IMG => $_, ERRSTR => undef }, "Imager"), @imgs;
   },
  );

Imager->register_writer
  (
   type=>'gif',
   single => 
   sub { 
     my ($im, $io, %hsh) = @_;

     $im->_set_opts(\%hsh, "i_", $im);
     $im->_set_opts(\%hsh, "gif_", $im);

     unless (i_writegif_wiol($io, \%hsh, $im->{IMG})) {
       $im->_set_error(Imager->_error_as_msg);
       return;
     }
     return $im;
   },
   multiple =>
   sub {
     my ($class, $io, $opts, @ims) = @_;

     Imager->_set_opts($opts, "gif_", @ims);

     my @work = map $_->{IMG}, @ims;
     unless (i_writegif_wiol($io, $opts, @work)) {
       Imager->_set_error(Imager->_error_as_msg);
       return;
     }

     return 1;
   },
  );

__END__

=head1 NAME

Imager::File::GIF - read and write GIF files

=head1 SYNOPSIS

  use Imager;

  my $img = Imager->new;
  $img->read(file=>"foo.gif")
    or die $img->errstr;

  $img->write(file => "foo.gif")
    or die $img->errstr;

=head1 DESCRIPTION

Imager's GIF support is documented in L<Imager::Files>.

=head1 AUTHOR

Tony Cook <tonyc@cpan.org>

=head1 SEE ALSO

Imager, Imager::Files.

=cut
