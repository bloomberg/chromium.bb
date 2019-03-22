package Imager::Filter::Flines;
use strict;
use Imager;
use vars qw($VERSION @ISA);

BEGIN {
  $VERSION = "0.03";
  
  require XSLoader;
  XSLoader::load('Imager::Filter::Flines', $VERSION);
}

Imager->register_filter(type=>'flines',
                        callsub => sub { my %hsh = @_; flines($hsh{image}) },
                        defaults => {},
                        callseq => [ 'image' ] );

1;

__END__

=head1 NAME

Imager::Filter::Flines - dim alternate lines to emulate a video display

=head1 SYNOPSIS

  use Imager;
  use Imager::Filter::Flines;

  $img->filter(type=>'flines');

=head1 DESCRIPTION

This is an adaption of the C<flines> dynamically loadable filter
provided in dynfilt/ in previous releases of Imager.

This filter has no parameters.

=head1 AUTHOR

Original by Arnar M. Hrafnkelsson.

Adapted by Tony Cook <tonyc@cpan.org>

=head1 SEE ALSO

Imager, Imager::Filters.

=cut
