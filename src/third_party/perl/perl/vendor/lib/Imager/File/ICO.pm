package Imager::File::ICO;
use strict;
use Imager;
use vars qw($VERSION @ISA);

BEGIN {
  $VERSION = "0.03";
  
  require XSLoader;
  XSLoader::load('Imager::File::ICO', $VERSION);
}

Imager->register_reader
  (
   type=>'ico',
   single => 
   sub { 
     my ($im, $io, %hsh) = @_;
     my $masked = 
       exists $hsh{ico_masked} ? $hsh{ico_masked} : 1;
     $im->{IMG} = i_readico_single($io, $hsh{page} || 0, $masked);

     unless ($im->{IMG}) {
       $im->_set_error(Imager->_error_as_msg);
       return;
     }
     return $im;
   },
   multiple =>
   sub {
     my ($io, %hsh) = @_;
     
     my $masked = 
       exists $hsh{ico_masked} ? $hsh{ico_masked} : 1;
     my @imgs = i_readico_multi($io, $masked);
     unless (@imgs) {
       Imager->_set_error(Imager->_error_as_msg);
       return;
     }
     return map { 
       bless { IMG => $_, DEBUG => $Imager::DEBUG, ERRSTR => undef }, 'Imager'
     } @imgs;
   },
  );

# the readers can read CUR files too
Imager->register_reader
  (
   type=>'cur',
   single => 
   sub { 
     my ($im, $io, %hsh) = @_;
     my $masked = 
       exists $hsh{ico_masked} ? $hsh{ico_masked} : 1;
     $im->{IMG} = i_readico_single($io, $hsh{page} || 0, $masked);

     unless ($im->{IMG}) {
       $im->_set_error(Imager->_error_as_msg);
       return;
     }
     return $im;
   },
   multiple =>
   sub {
     my ($io, %hsh) = @_;
     
     my $masked = 
       exists $hsh{ico_masked} ? $hsh{ico_masked} : 1;
     my @imgs = i_readico_multi($io, $masked);
     unless (@imgs) {
       Imager->_set_error(Imager->_error_as_msg);
       return;
     }
     return map { 
       bless { IMG => $_, DEBUG => $Imager::DEBUG, ERRSTR => undef }, 'Imager'
     } @imgs;
   },
  );

Imager->register_writer
  (
   type=>'ico',
   single => 
   sub { 
     my ($im, $io, %hsh) = @_;

     unless (i_writeico_wiol($io, $im->{IMG})) {
       $im->_set_error(Imager->_error_as_msg);
       return;
     }
     return $im;
   },
   multiple =>
   sub {
     my ($class, $io, $opts, @images) = @_;

     if (!i_writeico_multi_wiol($io, map $_->{IMG}, @images)) {
       Imager->_set_error(Imager->_error_as_msg);
       return;
     }

     return 1;
   },
  );

Imager->register_writer
  (
   type=>'cur',
   single => 
   sub { 
     my ($im, $io, %hsh) = @_;

     unless (i_writecur_wiol($io, $im->{IMG})) {
       $im->_set_error(Imager->_error_as_msg);
       return;
     }
     return $im;
   },
   multiple =>
   sub {
     my ($class, $io, $opts, @images) = @_;

     if (!i_writecur_multi_wiol($io, map $_->{IMG}, @images)) {
       Imager->_set_error(Imager->_error_as_msg);
       return;
     }

     return 1;
   },
  );

1;

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
