package PPI::Token::QuoteLike::Regexp;

=pod

=head1 NAME

PPI::Token::QuoteLike::Regexp - Regexp constructor quote-like operator

=head1 INHERITANCE

  PPI::Token::QuoteLike::Regexp
  isa PPI::Token::QuoteLike
      isa PPI::Token
          isa PPI::Element

=head1 DESCRIPTION

A C<PPI::Token::QuoteLike::Regexp> object represents the quote-like
operator used to construct anonymous L<Regexp> objects, as follows.

  # Create a Regexp object for a module filename
  my $module = qr/\.pm$/;

=head1 METHODS

The following methods are provided by this class,
beyond those provided by the parent L<PPI::Token::QuoteLike>,
L<PPI::Token> and L<PPI::Element> classes.

=cut

use strict;
use PPI::Token::QuoteLike          ();
use PPI::Token::_QuoteEngine::Full ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = qw{
		PPI::Token::_QuoteEngine::Full
		PPI::Token::QuoteLike
	};
}





#####################################################################
# PPI::Token::QuoteLike::Regexp Methods

=pod

=head2 get_match_string

The C<get_match_string> method returns the portion of the string that
will be compiled into the match portion of the regexp.

=cut

sub get_match_string {
	return $_[0]->_section_content( 0 );
}

=pod

=head2 get_substitute_string

The C<get_substitute_string> method always returns C<undef>, since
the C<qr{}> construction provides no substitution string. This method
is provided for orthogonality with C<PPI::Token::Regexp>.

=cut

sub get_substitute_string {
	return undef;
}

=pod

=head2 get_modifiers

The C<get_modifiers> method returns the modifiers that will be
compiled into the regexp.

=cut

sub get_modifiers {
	return $_[0]->_modifiers();
}

=pod

=head2 get_delimiters

The C<get_delimiters> method returns the delimiters of the string as an
array. The first and only element is the delimiters of the string to be
compiled into a match string.

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
