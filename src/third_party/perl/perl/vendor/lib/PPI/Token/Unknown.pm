package PPI::Token::Unknown;

=pod

=head1 NAME

PPI::Token::Unknown - Token of unknown or as-yet undetermined type

=head1 INHERITANCE

  PPI::Token::Unknown
  isa PPI::Token
      isa PPI::Element

=head1 DESCRIPTION

Object of the type C<PPI::Token::Unknown> exist primarily inside the
tokenizer, where they are temporarily brought into existing for a very
short time to represent a token that could be one of a number of types.

Generally, they only exist for a character or two, after which they are
resolved and converted into the correct type. For an object of this type
to survive the parsing process is considered a major bug.

Please report any C<PPI::Token::Unknown> you encounter in a L<PPI::Document>
object as a bug.

=cut

use strict;
use PPI::Token     ();
use PPI::Exception ();

use vars qw{$VERSION @ISA $CURLY_SYMBOL};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token';
	$CURLY_SYMBOL = qr{^\^[[:upper:]_]\w+\}};
}





#####################################################################
# Tokenizer Methods

sub __TOKENIZER__on_char {
	my $t    = $_[1];                                      # Tokenizer object
	my $c    = $t->{token}->{content};                     # Current token
	my $char = substr( $t->{line}, $t->{line_cursor}, 1 ); # Current character

	# Now, we split on the different values of the current content
	if ( $c eq '*' ) {
		if ( $char =~ /(?:(?!\d)\w|\:)/ ) {
			# Symbol (unless the thing before it is a number
			my $tokens = $t->_previous_significant_tokens(1);
			my $p0     = $tokens->[0];
			if ( $p0 and ! $p0->isa('PPI::Token::Number') ) {
				$t->{class} = $t->{token}->set_class( 'Symbol' );
				return 1;
			}
		}

		if ( $char eq '{' ) {
			# Get rest of line
			my $rest = substr( $t->{line}, $t->{line_cursor} + 1 );
			if ( $rest =~ m/$CURLY_SYMBOL/ ) {
				# control-character symbol (e.g. *{^_Foo})
				$t->{class} = $t->{token}->set_class( 'Magic' );
				return 1;
			} else {
				# Obvious GLOB cast
				$t->{class} = $t->{token}->set_class( 'Cast' );
				return $t->_finalize_token->__TOKENIZER__on_char( $t );
			}
		}

		if ( $char eq '$' ) {
			# Operator/operand-sensitive, multiple or GLOB cast
			my $_class = undef;
			my $tokens = $t->_previous_significant_tokens(1);
			my $p0     = $tokens->[0];
			if ( $p0 ) {
				# Is it a token or a number
				if ( $p0->isa('PPI::Token::Symbol') ) {
					$_class = 'Operator';
				} elsif ( $p0->isa('PPI::Token::Number') ) {
					$_class = 'Operator';
				} elsif (
					$p0->isa('PPI::Token::Structure')
					and
					$p0->content =~ /^(?:\)|\])$/
				) {
					$_class = 'Operator';
				} else {
					### This is pretty weak, there's
					### room for a dozen more tests
					### before going with a default.
					### Or even better, a proper
					### operator/operand method :(
					$_class = 'Cast';
				}
			} else {
				# Nothing before it, must be glob cast
				$_class = 'Cast';
			}

			# Set class and rerun
			$t->{class} = $t->{token}->set_class( $_class );
			return $t->_finalize_token->__TOKENIZER__on_char( $t );
		}

		if ( $char eq '*' || $char eq '=' ) {
			# Power operator '**' or mult-assign '*='
			$t->{class} = $t->{token}->set_class( 'Operator' );
			return 1;
		}

		$t->{class} = $t->{token}->set_class( 'Operator' );
		return $t->_finalize_token->__TOKENIZER__on_char( $t );



	} elsif ( $c eq '$' ) {
		if ( $char =~ /[a-z_]/i ) {
			# Symbol
			$t->{class} = $t->{token}->set_class( 'Symbol' );
			return 1;
		}

		if ( $PPI::Token::Magic::magic{ $c . $char } ) {
			# Magic variable
			$t->{class} = $t->{token}->set_class( 'Magic' );
			return 1;
		}

		if ( $char eq '{' ) {
			# Get rest of line
			my $rest = substr( $t->{line}, $t->{line_cursor} + 1 );
			if ( $rest =~ m/$CURLY_SYMBOL/ ) {
				# control-character symbol (e.g. ${^MATCH})
				$t->{class} = $t->{token}->set_class( 'Magic' );
				return 1;
			}
		}

		# Must be a cast
		$t->{class} = $t->{token}->set_class( 'Cast' );
		return $t->_finalize_token->__TOKENIZER__on_char( $t );



	} elsif ( $c eq '@' ) {
		if ( $char =~ /[\w:]/ ) {
			# Symbol
			$t->{class} = $t->{token}->set_class( 'Symbol' );
			return 1;
		}

		if ( $PPI::Token::Magic::magic{ $c . $char } ) {
			# Magic variable
			$t->{class} = $t->{token}->set_class( 'Magic' );
			return 1;
		}

		if ( $char eq '{' ) {
			# Get rest of line
			my $rest = substr( $t->{line}, $t->{line_cursor} + 1 );
			if ( $rest =~ m/$CURLY_SYMBOL/ ) {
				# control-character symbol (e.g. @{^_Foo})
				$t->{class} = $t->{token}->set_class( 'Magic' );
				return 1;
			}
		}

		# Must be a cast
		$t->{class} = $t->{token}->set_class( 'Cast' );
		return $t->_finalize_token->__TOKENIZER__on_char( $t );



	} elsif ( $c eq '%' ) {
		# Is it a number?
		if ( $char =~ /\d/ ) {
			# This is %2 (modulus number)
			$t->{class} = $t->{token}->set_class( 'Operator' );
			return $t->_finalize_token->__TOKENIZER__on_char( $t );
		}

		# Is it a magic variable?
		if ( $char eq '^' || $PPI::Token::Magic::magic{ $c . $char } ) {
			$t->{class} = $t->{token}->set_class( 'Magic' );
			return 1;
		}

		# Is it a symbol?
		if ( $char =~ /[\w:]/ ) {
			$t->{class} = $t->{token}->set_class( 'Symbol' );
			return 1;
		}

		if ( $char eq '{' ) {
			# Get rest of line
			my $rest = substr( $t->{line}, $t->{line_cursor} + 1 );
			if ( $rest =~ m/$CURLY_SYMBOL/ ) {
				# control-character symbol (e.g. @{^_Foo})
				$t->{class} = $t->{token}->set_class( 'Magic' );
				return 1;
			}
		}

		if ( $char =~ /[\$@%*{]/ ) {
			# It's a cast
			$t->{class} = $t->{token}->set_class( 'Cast' );
			return $t->_finalize_token->__TOKENIZER__on_char( $t );

		}

		# Probably the mod operator
		$t->{class} = $t->{token}->set_class( 'Operator' );
		return $t->{class}->__TOKENIZER__on_char( $t );



	} elsif ( $c eq '&' ) {
		# Is it a number?
		if ( $char =~ /\d/ ) {
			# This is &2 (bitwise-and number)
			$t->{class} = $t->{token}->set_class( 'Operator' );
			return $t->_finalize_token->__TOKENIZER__on_char( $t );
		}

		# Is it a symbol
		if ( $char =~ /[\w:]/ ) {
			$t->{class} = $t->{token}->set_class( 'Symbol' );
			return 1;
		}

		if ( $char =~ /[\$@%{]/ ) {
			# The ampersand is a cast
			$t->{class} = $t->{token}->set_class( 'Cast' );
			return $t->_finalize_token->__TOKENIZER__on_char( $t );
		}

		# Probably the binary and operator
		$t->{class} = $t->{token}->set_class( 'Operator' );
		return $t->{class}->__TOKENIZER__on_char( $t );



	} elsif ( $c eq '-' ) {
		if ( $char =~ /\d/o ) {
			# Number
			$t->{class} = $t->{token}->set_class( 'Number' );
			return 1;
		}

		if ( $char eq '.' ) {
			# Number::Float
			$t->{class} = $t->{token}->set_class( 'Number::Float' );
			return 1;
		}

		if ( $char =~ /[a-zA-Z]/ ) {
			$t->{class} = $t->{token}->set_class( 'DashedWord' );
			return 1;
		}

		# The numeric negative operator
		$t->{class} = $t->{token}->set_class( 'Operator' );
		return $t->{class}->__TOKENIZER__on_char( $t );



	} elsif ( $c eq ':' ) {
		if ( $char eq ':' ) {
			# ::foo style bareword
			$t->{class} = $t->{token}->set_class( 'Word' );
			return 1;
		}

		# Now, : acts very very differently in different contexts.
		# Mainly, we need to find out if this is a subroutine attribute.
		# We'll leave a hint in the token to indicate that, if it is.
		if ( $_[0]->__TOKENIZER__is_an_attribute( $t ) ) {
			# This : is an attribute indicator
			$t->{class} = $t->{token}->set_class( 'Operator' );
			$t->{token}->{_attribute} = 1;
			return $t->_finalize_token->__TOKENIZER__on_char( $t );
		}

		# It MIGHT be a label, but its probably the ?: trinary operator
		$t->{class} = $t->{token}->set_class( 'Operator' );
		return $t->{class}->__TOKENIZER__on_char( $t );
	}

	# erm...
	PPI::Exception->throw('Unknown value in PPI::Token::Unknown token');
}

# Are we at a location where a ':' would indicate a subroutine attribute
sub __TOKENIZER__is_an_attribute {
	my $t      = $_[1]; # Tokenizer object
	my $tokens = $t->_previous_significant_tokens(3);
	my $p0     = $tokens->[0];

	# If we just had another attribute, we are also an attribute
	return 1 if $p0->isa('PPI::Token::Attribute');

	# If we just had a prototype, then we are an attribute
	return 1 if $p0->isa('PPI::Token::Prototype');

	# Other than that, we would need to have had a bareword
	return '' unless $p0->isa('PPI::Token::Word');

	# We could be an anonymous subroutine
	if ( $p0->isa('PPI::Token::Word') and $p0->content eq 'sub' ) {
		return 1;
	}

	# Or, we could be a named subroutine
	my $p1 = $tokens->[1];
	my $p2 = $tokens->[2];
	if (
		$p1->isa('PPI::Token::Word')
		and
		$p1->content eq 'sub'
		and (
			$p2->isa('PPI::Token::Structure')
			or (
				$p2->isa('PPI::Token::Whitespace')
				and
				$p2->content eq ''
			)
		)
	) {
		return 1;
	}

	# We arn't an attribute
	'';	
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
