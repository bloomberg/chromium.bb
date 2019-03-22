package PPI::Token::Structure;

=pod

=head1 NAME

PPI::Token::Structure - Token class for characters that define code structure

=head1 INHERITANCE

  PPI::Token::Structure
  isa PPI::Token
      isa PPI::Element

=head1 DESCRIPTION

The C<PPI::Token::Structure> class is used for tokens that control the
generally tree structure or code.

This consists of seven characters. These are the six brace characters from
the "round", "curly" and "square" pairs, plus the semi-colon statement
separator C<;>.

=head1 METHODS

This class has no methods beyond what is provided by its
L<PPI::Token> and L<PPI::Element> parent classes.

=cut

use strict;
use PPI::Token ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token';
}

# Set the matching braces, done as an array
# for slightly faster lookups.
use vars qw{@MATCH @OPENS @CLOSES};
BEGIN {
	$MATCH[ord '{']  = '}';
	$MATCH[ord '}']  = '{';
	$MATCH[ord '[']  = ']';
	$MATCH[ord ']']  = '[';
	$MATCH[ord '(']  = ')';
	$MATCH[ord ')']  = '(';

	$OPENS[ord '{']  = 1;
	$OPENS[ord '[']  = 1;
	$OPENS[ord '(']  = 1;

	$CLOSES[ord '}'] = 1;
	$CLOSES[ord ']'] = 1;
	$CLOSES[ord ')'] = 1;
}





#####################################################################
# Tokenizer Methods

sub __TOKENIZER__on_char {
	# Structures are one character long, always.
	# Finalize and process again.
	$_[1]->_finalize_token->__TOKENIZER__on_char( $_[1] );
}

sub __TOKENIZER__commit {
	my $t = $_[1];
	$t->_new_token( 'Structure', substr( $t->{line}, $t->{line_cursor}, 1 ) );
	$t->_finalize_token;
	0;
}





#####################################################################
# Lexer Methods

# For a given brace, find its opposing pair
sub __LEXER__opposite {
	$MATCH[ord $_[0]->{content} ];
}





#####################################################################
# PPI::Element Methods

# There is a unusual situation in regards to "siblings".
#
# As an Element, braces sit outside the normal tree structure, and in
# this context they NEVER have siblings.
#
# However, as tokens they DO have siblings.
#
# As such, we need special versions of _all_ of the sibling methods to
# handle this.
#
# Statement terminators do not have these problems, and for them sibling
# calls work as normal, and so they can just be passed upwards.

sub next_sibling {
	return $_[0]->SUPER::next_sibling if $_[0]->{content} eq ';';
	return '';
}

sub snext_sibling {
	return $_[0]->SUPER::snext_sibling if $_[0]->{content} eq ';';
	return '';
}

sub previous_sibling {
	return $_[0]->SUPER::previous_sibling if $_[0]->{content} eq ';';
	return '';
}

sub sprevious_sibling {
	return $_[0]->SUPER::sprevious_sibling if $_[0]->{content} eq ';';
	return '';
}

sub next_token {
	my $self = shift;
	return $self->SUPER::next_token if $self->{content} eq ';';
	my $structure = $self->parent or return '';

	# If this is an opening brace, descend down into our parent
	# structure, if it has children.
	if ( $OPENS[ ord $self->{content} ] ) {
		my $child = $structure->child(0);
		if ( $child ) {
			# Decend deeper, or return if it is a token
			return $child->isa('PPI::Token') ? $child : $child->first_token;
		} elsif ( $structure->finish ) {
			# Empty structure, so next is closing brace
			return $structure->finish;
		}

		# Anything that slips through to here is a structure
		# with an opening brace, but no closing brace, so we
		# just have to go with it, and continue as we would
		# if we started with a closing brace.
	}

	# We can use the default implement, if we call it from the
	# parent structure of the closing brace.
	$structure->next_token;
}

sub previous_token {
	my $self = shift;
	return $self->SUPER::previous_token if $self->{content} eq ';';
	my $structure = $self->parent or return '';

	# If this is a closing brace, descend down into our parent
	# structure, if it has children.
	if ( $CLOSES[ ord $self->{content} ] ) {
		my $child = $structure->child(-1);
		if ( $child ) {
			# Decend deeper, or return if it is a token
			return $child->isa('PPI::Token') ? $child : $child->last_token;
		} elsif ( $structure->start ) {
			# Empty structure, so next is closing brace
			return $structure->start;
		}

		# Anything that slips through to here is a structure
		# with a closing brace, but no opening brace, so we
		# just have to go with it, and continue as we would
		# if we started with a opening brace.
	}

	# We can use the default implement, if we call it from the
	# parent structure of the closing brace.
	$structure->previous_token;
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
