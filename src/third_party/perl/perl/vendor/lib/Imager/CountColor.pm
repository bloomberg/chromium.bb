package Imager::CountColor;
use strict;
use Imager;
use vars qw($VERSION @ISA @EXPORT_OK);
require Exporter;
@EXPORT_OK = 'count_color';

BEGIN {
  $VERSION = "0.02";
  @ISA = qw(Exporter);
  
  require XSLoader;
  XSLoader::load('Imager::CountColor', $VERSION);
}

1;

__END__

=head1 NAME

Imager::CountColor - demonstrates writing a simple function using Imager.

=head1 SYNOPSIS

  use Imager;
  use Imager::CountColor;
  my $im = Imager->new(...); # some Imager image
  ...; # some sort of manipulation
  print count_color($im, $color_object);

=head1 DESCRIPTION

This module is a simple demonstration of how to create an XS module
that works with Imager objects.

You may want to copy the source for this module as a start.

=head1 SEE ALSO

Imager, Imager::Filter::DynTest

=head1 AUTHOR

Tony Cook <tonyc@cpan.org>

=cut


  
