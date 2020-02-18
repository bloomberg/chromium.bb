package Imager::Filter::Mandelbrot;
use strict;
use Imager;
use vars qw($VERSION @ISA);

BEGIN {
  $VERSION = "0.04";
  
  require XSLoader;
  XSLoader::load('Imager::Filter::Mandelbrot', $VERSION);
}

sub _mandelbrot {
  my %hsh = @_;

  mandelbrot($hsh{image}, $hsh{minx}, $hsh{miny}, $hsh{maxx}, $hsh{maxy}, $hsh{maxiter});
}

my %defaults =
  (
   minx => -2.5,
   maxx => 1.5,
   miny => -1.5,
   maxy => 1.5,
   maxiter => 256,
  );

my @callseq = qw/image minx miny maxx maxy maxiter/;

Imager->register_filter(type=>'mandelbrot',
                        callsub => \&_mandelbrot,
                        defaults => \%defaults,
                        callseq => \@callseq);

1;

__END__

=head1 NAME

Imager::Filter::Mandelbrot - filter that renders the Mandelbrot set.

=head1 SYNOPSIS

  use Imager;
  use Imager::Filter::Mandelbrot;

  $img->filter(type=>'mandelbrot', ...);

=head1 DESCRIPTION

This is a expansion of the C<mandelbrot> dynamically loadable filter
provided in C<dynfilt> in previous releases of Imager.

Valid filter parameters are:

=over

=item *

C<minx>, C<maxx> - the range of x values to render.  Defaults: -2.5, 1.5.

=item *

C<miny>, C<maxy> - the range of y values to render.  Defaults: -1.5, 1.5

=item *

C<maxiter> - the maximum number of iterations to perform when checking
if the sequence tend towards infinity.

=back

=head1 AUTHOR

Original by Arnar M. Hrafnkelsson.

Adapted and expanded by Tony Cook <tonyc@cpan.org>

=head1 SEE ALSO

Imager, Imager::Filters.

=cut
