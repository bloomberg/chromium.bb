package PPI::Token::Word;

=pod

=head1 NAME

PPI::Token::Word - The generic "word" Token

=head1 INHERITANCE

  PPI::Token::Word
  isa PPI::Token
      isa PPI::Element

=head1 DESCRIPTION

A C<PPI::Token::Word> object is a PPI-specific representation of several
different types of word-like things, and is one of the most common Token
classes found in typical documents.

Specifically, it includes not only barewords, but also any other valid
Perl identifier including non-operator keywords and core functions, and
any include C<::> separators inside it, as long as it fits the
format of a class, function, etc.

=head1 METHODS

There are no methods available for C<PPI::Token::Word> beyond those
provided by its L<PPI::Token> and L<PPI::Element> parent
classes.

We expect to add additional methods to help further resolve a Word as
a function, method, etc over time.  If you need such a thing right
now, look at L<Perl::Critic::Utils>.

=cut

use strict;
use PPI::Token ();
use PPI::Singletons qw' %OPERATOR %QUOTELIKE %KEYWORDS ';

our $VERSION = '1.269'; # VERSION

our @ISA = "PPI::Token";

=pod

=head2 literal

Returns the value of the Word as a string.  This assumes (often
incorrectly) that the Word is a bareword and not a function, method,
keyword, etc.  This differs from C<content> because C<Foo'Bar> expands
to C<Foo::Bar>.

=cut

sub literal {
	my $self = shift;
	my $word = $self->content;

	# Expand Foo'Bar to Foo::Bar
	$word =~ s/\'/::/g;

	return $word;
}

=pod

=head2 method_call

Answers whether this is the name of a method in a method call. Returns true if
yes, false if no, and nothing if unknown.

=cut

sub method_call {
	my $self = shift;

	my $previous = $self->sprevious_sibling;
	if (
		$previous
		and
		$previous->isa('PPI::Token::Operator')
		and
		$previous->content eq '->'
	) {
		return 1;
	}

	my $snext = $self->snext_sibling;
	return 0 unless $snext;

	if (
		$snext->isa('PPI::Structure::List')
		or
		$snext->isa('PPI::Token::Structure')
		or
		$snext->isa('PPI::Token::Operator')
		and (
			$snext->content eq ','
			or
			$snext->content eq '=>'
		)
	) {
		return 0;
	}

	if (
		$snext->isa('PPI::Token::Word')
		and
		$snext->content =~ m< \w :: \z >xms
	) {
		return 1;
	}

	return;
}


sub __TOKENIZER__on_char {
	my $class = shift;
	my $t     = shift;

	# Suck in till the end of the bareword
	pos $t->{line} = $t->{line_cursor};
	if ( $t->{line} =~ m/\G(\w+(?:(?:\'|::)\w+)*(?:::)?)/gc ) {
		my $word = $1;
		# Special Case: If we accidentally treat eq'foo' like
		# the word "eq'foo", then just make 'eq' (or whatever
		# else is in the %KEYWORDS hash.
		if ( $word =~ /^(\w+)'/ && $KEYWORDS{$1} ) {
		    $word = $1;
		}
		$t->{token}->{content} .= $word;
		$t->{line_cursor} += length $word;

	}

	# We might be a subroutine attribute.
	if ( __current_token_is_attribute($t) ) {
		$t->{class} = $t->{token}->set_class( 'Attribute' );
		return $t->{class}->__TOKENIZER__commit( $t );
	}

	my $word = $t->{token}->{content};
	if ( $KEYWORDS{$word} ) {
		# Check for a Perl keyword that is forced to be a normal word instead
		if ( $t->__current_token_is_forced_word ) {
			$t->{class} = $t->{token}->set_class( 'Word' );
			return $t->{class}->__TOKENIZER__on_char( $t );
		}

		# Check for a quote like operator. %QUOTELIKE must be subset of %KEYWORDS
		if ( $QUOTELIKE{$word} ) {
			$t->{class} = $t->{token}->set_class( $QUOTELIKE{$word} );
			return $t->{class}->__TOKENIZER__on_char( $t );
		}

		# Or one of the word operators. %OPERATOR must be subset of %KEYWORDS
		if ( $OPERATOR{$word} ) {
			$t->{class} = $t->{token}->set_class( 'Operator' );
			return $t->_finalize_token->__TOKENIZER__on_char( $t );
		}
	}

	# Unless this is a simple identifier, at this point
	# it has to be a normal bareword
	if ( $word =~ /\:/ ) {
		return $t->_finalize_token->__TOKENIZER__on_char( $t );
	}

	# If the NEXT character in the line is a colon, this
	# is a label.
	my $char = substr( $t->{line}, $t->{line_cursor}, 1 );
	if ( $char eq ':' ) {
		$t->{token}->{content} .= ':';
		$t->{line_cursor}++;
		$t->{class} = $t->{token}->set_class( 'Label' );

	# If not a label, '_' on its own is the magic filehandle
	} elsif ( $word eq '_' ) {
		$t->{class} = $t->{token}->set_class( 'Magic' );

	}

	# Finalise and process the character again
	$t->_finalize_token->__TOKENIZER__on_char( $t );
}



# We are committed to being a bareword.
# Or so we would like to believe.
sub __TOKENIZER__commit {
	my ($class, $t) = @_;

	# Our current position is the first character of the bareword.
	# Capture the bareword.
	pos $t->{line} = $t->{line_cursor};
	unless ( $t->{line} =~ m/\G((?!\d)\w+(?:(?:\'|::)\w+)*(?:::)?)/gc ) {
		# Programmer error
		die sprintf "Fatal error... regex failed to match in '%s' when expected", substr $t->{line}, $t->{line_cursor};
	}

	# Special Case: If we accidentally treat eq'foo' like the word "eq'foo",
	# then unwind it and just make it 'eq' (or the other stringy comparitors)
	my $word = $1;
	if ( $word =~ /^(\w+)'/ && $KEYWORDS{$1} ) {
	    $word = $1;
	}

	# Advance the position one after the end of the bareword
	$t->{line_cursor} += length $word;

	# We might be a subroutine attribute.
	if ( __current_token_is_attribute($t) ) {
		$t->_new_token( 'Attribute', $word );
		return ($t->{line_cursor} >= $t->{line_length}) ? 0
			: $t->{class}->__TOKENIZER__on_char($t);
	}

	# Check for the end of the file
	if ( $word eq '__END__' ) {
		# Create the token for the __END__ itself
		$t->_new_token( 'Separator', $1 );
		$t->_finalize_token;

		# Move into the End zone (heh)
		$t->{zone} = 'PPI::Token::End';

		# Add the rest of the line as a comment, and a whitespace newline
		# Anything after the __END__ on the line is "ignored". So we must
		# also ignore it, by turning it into a comment.
		my $end_rest = substr( $t->{line}, $t->{line_cursor} );
		$t->{line_cursor} = length $t->{line};
		if ( $end_rest =~ /\n$/ ) {
			chomp $end_rest;
			$t->_new_token( 'Comment', $end_rest ) if length $end_rest;
			$t->_new_token( 'Whitespace', "\n" );
		} else {
			$t->_new_token( 'Comment', $end_rest ) if length $end_rest;
		}
		$t->_finalize_token;

		return 0;
	}

	# Check for the data section
	if ( $word eq '__DATA__' ) {
		# Create the token for the __DATA__ itself
		$t->_new_token( 'Separator', "$1" );
		$t->_finalize_token;

		# Move into the Data zone
		$t->{zone} = 'PPI::Token::Data';

		# Add the rest of the line as the Data token
		my $data_rest = substr( $t->{line}, $t->{line_cursor} );
		$t->{line_cursor} = length $t->{line};
		if ( $data_rest =~ /\n$/ ) {
			chomp $data_rest;
			$t->_new_token( 'Comment', $data_rest ) if length $data_rest;
			$t->_new_token( 'Whitespace', "\n" );
		} else {
			$t->_new_token( 'Comment', $data_rest ) if length $data_rest;
		}
		$t->_finalize_token;

		return 0;
	}

	my $token_class;
	if ( $word =~ /\:/ ) {
		# Since it's not a simple identifier...
		$token_class = 'Word';

	} elsif ( $KEYWORDS{$word} and $t->__current_token_is_forced_word ) {
		$token_class = 'Word';

	} elsif ( $QUOTELIKE{$word} ) {
		# Special Case: A Quote-like operator
		$t->_new_token( $QUOTELIKE{$word}, $word );
		return ($t->{line_cursor} >= $t->{line_length}) ? 0
			: $t->{class}->__TOKENIZER__on_char( $t );

	} elsif ( $OPERATOR{$word} && ($word ne 'x' || $t->_current_x_is_operator) ) {
		# Word operator
		$token_class = 'Operator';

	} else {
		# Get tokens early to be sure to not disturb state set up by pos and m//gc.
		my @tokens = $t->_previous_significant_tokens(1);

		# If the next character is a ':' then it's a label...
		pos $t->{line} = $t->{line_cursor};
		if ( $t->{line} =~ m/\G(\s*:)(?!:)/gc ) {
			if ( $tokens[0] and $tokens[0]->{content} eq 'sub' ) {
				# ... UNLESS it's after 'sub' in which
				# case it is a sub name and an attribute
				# operator.
				# We COULD have checked this at the top
				# level of checks, but this would impose
				# an additional performance per-word
				# penalty, and every other case where the
				# attribute operator doesn't directly
				# touch the object name already works.
				$token_class = 'Word';
			} else {
				$word .= $1;
				$t->{line_cursor} += length($1);
				$token_class = 'Label';
			}
		} elsif ( $word eq '_' ) {
			$token_class = 'Magic';
		} else {
			$token_class = 'Word';
		}
	}

	# Create the new token and finalise
	$t->_new_token( $token_class, $word );
	if ( $t->{line_cursor} >= $t->{line_length} ) {
		# End of the line
		$t->_finalize_token;
		return 0;
	}
	$t->_finalize_token->__TOKENIZER__on_char($t);
}



# Is the current Word really a subroutine attribute?
sub __current_token_is_attribute {
	my ( $t ) = @_;
	my @tokens = $t->_previous_significant_tokens(1);
	return (
		$tokens[0]
		and (
			# hint from tokenizer
			$tokens[0]->{_attribute}
			# nothing between attribute and us except whitespace
			or $tokens[0]->isa('PPI::Token::Attribute')
		)
	);
}

1;

=pod

=head1 TO DO

- Add C<function>, C<method> etc detector methods

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
