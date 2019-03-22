package PPI::Token::Regexp;

=pod

=head1 NAME

PPI::Token::Regexp - Regular expression abstract base class

=head1 INHERITANCE

  PPI::Token::Regexp
  isa PPI::Token
      isa PPI::Element

=head1 DESCRIPTION

The C<PPI::Token::Regexp> class is never instantiated, and simply
provides a common abstract base class for the three regular expression
classes. These being:

=over 2

=item m// - L<PPI::Token::Regexp::Match>

=item s/// - L<PPI::Token::Regexp::Substitute>

=item tr/// - L<PPI::Token::Regexp::Transliterate>

=back

The names are hopefully obvious enough not to have to explain what
each class is. See their pages for more details.

To save some confusion, it's worth pointing out here that C<qr//> is
B<not> a regular expression (which PPI takes to mean something that
will actually examine or modify a string), but rather a quote-like
operator that acts as a constructor for compiled L<Regexp> objects. 

=head1 METHODS

The following methods are inherited by this class' offspring:

=cut

use strict;
use PPI::Token ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token';
}





#####################################################################
# PPI::Token::Regexp Methods

=pod

=head2 get_match_string

The C<get_match_string> method returns the portion of the regexp that
performs the match.

=cut

sub get_match_string {
	return $_[0]->_section_content( 0 );
}

=pod

=head2 get_substitute_string

The C<get_substitute_string> method returns the portion of the regexp
that is substituted for the match, if any.  If the regexp does not
substitute, C<undef> is returned.

=cut

sub get_substitute_string {
	return $_[0]->_section_content( 1 );
}

=pod

=head2 get_modifiers

The C<get_modifiers> method returns the modifiers of the regexp.

=cut

sub get_modifiers {
	return $_[0]->_modifiers();
}

=pod

=head2 get_delimiters

The C<get_delimiters> method returns the delimiters of the regexp as
an array. The first element is the delimiters of the match string, and
the second element (if any) is the delimiters of the substitute string
(if any).

=cut

sub get_delimiters {
	return $_[0]->_delimiters();
}


1;

=pod

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
