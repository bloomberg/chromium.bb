package Imager::Color::Float;

use Imager;
use strict;
use vars qw($VERSION);

$VERSION = "1.005";

# It's just a front end to the XS creation functions.


# Parse color spec into an a set of 4 colors

sub _pspec {
  return (@_,1) if @_ == 3;
  return (@_    ) if @_ == 4;
  if ($_[0] =~ 
      /^\#?([\da-f][\da-f])([\da-f][\da-f])([\da-f][\da-f])([\da-f][\da-f])/i) {
    return (hex($1)/255,hex($2)/255,hex($3)/255,hex($4)/255);
  }
  if ($_[0] =~ /^\#?([\da-f][\da-f])([\da-f][\da-f])([\da-f][\da-f])/i) {
    return (hex($1)/255,hex($2)/255,hex($3)/255,1);
  }
  return ();
}

sub new {
  shift; # get rid of class name.
  my @arg = _pspec(@_);
  return @arg ? new_internal($arg[0],$arg[1],$arg[2],$arg[3]) : ();
}

sub set {
  my $self = shift;
  my @arg = _pspec(@_);
  return @arg ? set_internal($self, $arg[0],$arg[1],$arg[2],$arg[3]) : ();
}

sub CLONE_SKIP { 1 }

1;

__END__

=head1 NAME

Imager::Color::Float - Rough floating point sample color handling

=head1 SYNOPSIS

  $color = Imager::Color->new($red, $green, $blue);
  $color = Imager::Color->new($red, $green, $blue, $alpha);
  $color = Imager::Color->new("#C0C0FF"); # html color specification

  $color->set($red, $green, $blue);
  $color->set($red, $green, $blue, $alpha);
  $color->set("#C0C0FF"); # html color specification

  ($red, $green, $blue, $alpha) = $color->rgba();
  @hsv = $color->hsv(); # not implemented but proposed

  $color->info();


=head1 DESCRIPTION

This module handles creating color objects used by Imager.  The idea
is that in the future this module will be able to handle color space
calculations as well.

A floating point Imager color consists of up to four components, each
in the range 0.0 to 1.0. Unfortunately the meaning of the components
can change depending on the type of image you're dealing with:

=over

=item *

for 3 or 4 channel images the color components are red, green, blue,
alpha.

=item *

for 1 or 2 channel images the color components are gray, alpha, with
the other two components ignored.

=back

An alpha value of zero is fully transparent, an alpha value of 1.0 is
fully opaque.

=head1 METHODS

=over 4

=item new

This creates a color object to pass to functions that need a color argument.

=item set

This changes an already defined color.  Note that this does not affect any places
where the color has been used previously.

=item rgba()

This returns the red, green, blue and alpha channels of the color the
object contains.

=item info

Calling info merely dumps the relevant color to the log.

=back

=head1 AUTHOR

Arnar M. Hrafnkelsson, addi@umich.edu
And a great deal of help from others - see the C<README> for a complete
list.

=head1 SEE ALSO

Imager(3), Imager::Color.

http://imager.perl.org/

=cut

