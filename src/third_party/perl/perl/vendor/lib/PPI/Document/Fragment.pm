package PPI::Document::Fragment;

=pod

=head1 NAME

PPI::Document::Fragment - A fragment of a Perl Document

=head1 DESCRIPTION

In some situations you might want to work with a fragment of a larger
document. C<PPI::Document::Fragment> is a class intended for this purpose.
It is functionally almost identical to a normal L<PPI::Document>, except
that it is not possible to get line/column positions for the elements
within it, and it does not represent a scope.

=head1 METHODS

=cut

use strict;
use PPI::Document ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Document';
}





#####################################################################
# PPI::Document Methods

=pod

=head2 index_locations

Unlike when called on a PPI::Document object, you should not be attempting
to find locations of things within a PPI::Document::Fragment, and thus any
call to the C<index_locations> will print a warning and return C<undef>
instead of attempting to index the locations of the Elements.

=cut

# There's no point indexing a fragment
sub index_locations {
	warn "Useless attempt to index the locations of a document fragment";
	undef;
}





#####################################################################
# PPI::Element Methods

# We are not a scope boundary
### XS -> PPI/XS.xs:_PPI_Document_Fragment__scope 0.903+
sub scope { '' }

1;

=pod

=head1 TO DO

Integrate this into the rest of PPI so it has actual practical uses. The most
obvious would be to implement arbitrary cut/copy/paste more easily.

=head1 SUPPORT

See the L<support section|PPI/SUPPORT> in the main module.

=head1 AUTHOR

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

=head1 COPYRIGHT

Copyright 2001 - 2011 Adam Kennedy.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
