package Imager::Font::Type1;
use strict;
use Imager::Font::T1;
use vars qw(@ISA $VERSION);
@ISA = qw(Imager::Font::FT2);

$VERSION = "1.012";

1;

__END__

=head1 NAME

  Imager::Font::Type1 - low-level functions for T1Lib text output

=head1 DESCRIPTION

This is a simple wrapper around Imager::Font::T1 for backwards
compatibility.

=head1 AUTHOR

Tony Cook <tonyc@cpan.org>

=cut
