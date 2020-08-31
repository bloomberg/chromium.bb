package Imager::Font::FreeType2;
use strict;
use Imager::Font::FT2;
use vars qw(@ISA $VERSION);
@ISA = qw(Imager::Font::FT2);

$VERSION = "1.020";


1;

__END__

=head1 NAME

  Imager::Font::FreeType2 - low-level functions for FreeType2 text output

=head1 DESCRIPTION

Imager::Font creates a Imager::Font::FreeType2 object when asked to.

See Imager::Font to see how to use this type.

This class provides low-level functions that require the caller to
perform data validation.

This driver supports:

=over

=item transform()

=item dpi()

=item draw()

The following parameters:

=over

=item *

C<utf8>

=item *

C<vlayout>

=item *

C<sizew>

=back

=back

=head2 Special behaviors

If you call transform() to set a transformation matrix, hinting will
be switched off.  This prevents sudden jumps in the size of the text
caused by the hinting when the transformation is the identity matrix.
If for some reason you want hinting enabled, use
$font->hinting(hinting=>1) to re-enable hinting.  This will need to be
called after I<each> call to transform().

=head1 AUTHOR

Addi, Tony

=cut
