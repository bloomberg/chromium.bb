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
use PPI::Singletons qw' %MAGIC $CURLY_SYMBOL ';

our $VERSION = '1.269'; # VERSION

our @ISA = "PPI::Token";






#####################################################################
# Tokenizer Methods

sub __TOKENIZER__on_char {
	my ( $self, $t ) = @_;                                 # Self and Tokenizer
	my $c    = $t->{token}->{content};                     # Current token
	my $char = substr( $t->{line}, $t->{line_cursor}, 1 ); # Current character

	# Now, we split on the different values of the current content
	if ( $c eq '*' ) {
		# Is it a number?
		if ( $char =~ /\d/ ) {
			# bitwise operator
			$t->{class} = $t->{token}->set_class( 'Operator' );
			return $t->_finalize_token->__TOKENIZER__on_char( $t );
		}

		if ( $char =~ /[\w:]/ ) {
			# Symbol (unless the thing before it is a number
			my ( $prev ) = $t->_previous_significant_tokens(1);
			if ( not $prev or not $prev->isa('PPI::Token::Number') ) {
				$t->{class} = $t->{token}->set_class( 'Symbol' );
				return 1;
			}
		}

		if ( $char eq '{' ) {
			# Get rest of line
			pos $t->{line} = $t->{line_cursor} + 1;
			if ( $t->{line} =~ m/$CURLY_SYMBOL/gc ) {
				# control-character symbol (e.g. *{^_Foo})
				$t->{class} = $t->{token}->set_class( 'Magic' );
				return 1;
			}
		}

		# Postfix dereference: ->**
		if ( $char eq '*' ) {
			my ( $prev ) = $t->_previous_significant_tokens(1);
			if ( $prev and $prev->isa('PPI::Token::Operator') and $prev->content eq '->' ) {
				$t->{class} = $t->{token}->set_class( 'Cast' );
				return 1;
			}
		}

		if ( $char eq '*' || $char eq '=' ) {
			# Power operator '**' or mult-assign '*='
			$t->{class} = $t->{token}->set_class( 'Operator' );
			return 1;
		}

		return $self->_as_cast_or_op($t) if $self->_is_cast_or_op($char);

		$t->{class} = $t->{token}->set_class( 'Operator' );
		return $t->_finalize_token->__TOKENIZER__on_char( $t );



	} elsif ( $c eq '$' ) {
		# Postfix dereference: ->$* ->$#*
		if ( $char eq '*' || $char eq '#' ) {
			my ( $prev ) = $t->_previous_significant_tokens(1);
			if ( $prev and $prev->isa('PPI::Token::Operator') and $prev->content eq '->' ) {
				$t->{class} = $t->{token}->set_class( 'Cast' );
				return 1;
			}
		}

		if ( $char =~ /[a-z_]/i ) {
			# Symbol
			$t->{class} = $t->{token}->set_class( 'Symbol' );
			return 1;
		}

		if ( $MAGIC{ $c . $char } ) {
			# Magic variable
			$t->{class} = $t->{token}->set_class( 'Magic' );
			return 1;
		}

		if ( $char eq '{' ) {
			# Get rest of line
			pos $t->{line} = $t->{line_cursor} + 1;
			if ( $t->{line} =~ m/$CURLY_SYMBOL/gc ) {
				# control-character symbol (e.g. ${^MATCH})
				$t->{class} = $t->{token}->set_class( 'Magic' );
				return 1;
			}
		}

		# Must be a cast
		$t->{class} = $t->{token}->set_class( 'Cast' );
		return $t->_finalize_token->__TOKENIZER__on_char( $t );



	} elsif ( $c eq '@' ) {
		# Postfix dereference: ->@*
		if ( $char eq '*' ) {
			my ( $prev ) = $t->_previous_significant_tokens(1);
			if ( $prev and $prev->isa('PPI::Token::Operator') and $prev->content eq '->' ) {
				$t->{class} = $t->{token}->set_class( 'Cast' );
				return 1;
			}
		}

		if ( $char =~ /[\w:]/ ) {
			# Symbol
			$t->{class} = $t->{token}->set_class( 'Symbol' );
			return 1;
		}

		if ( $MAGIC{ $c . $char } ) {
			# Magic variable
			$t->{class} = $t->{token}->set_class( 'Magic' );
			return 1;
		}

		if ( $char eq '{' ) {
			# Get rest of line
			pos $t->{line} = $t->{line_cursor} + 1;
			if ( $t->{line} =~ m/$CURLY_SYMBOL/gc ) {
				# control-character symbol (e.g. @{^_Foo})
				$t->{class} = $t->{token}->set_class( 'Magic' );
				return 1;
			}
		}

		# Must be a cast
		$t->{class} = $t->{token}->set_class( 'Cast' );
		return $t->_finalize_token->__TOKENIZER__on_char( $t );



	} elsif ( $c eq '%' ) {
		# Postfix dereference: ->%* ->%[...]
		if ( $char eq '*' || $char eq '[' ) {
			my ( $prev ) = $t->_previous_significant_tokens(1);
			if ( $prev and $prev->isa('PPI::Token::Operator') and $prev->content eq '->' ) {
				if ( $char eq '*' ) {
					$t->{class} = $t->{token}->set_class( 'Cast' );
					return 1;
				}
				if ( $char eq '[' ) {
					$t->{class} = $t->{token}->set_class( 'Cast' );
					return $t->_finalize_token->__TOKENIZER__on_char( $t );
				}
			}
		}

		# Is it a number?
		if ( $char =~ /\d/ ) {
			# bitwise operator
			$t->{class} = $t->{token}->set_class( 'Operator' );
			return $t->_finalize_token->__TOKENIZER__on_char( $t );
		}

		# Is it a magic variable?
		if ( $char eq '^' || $MAGIC{ $c . $char } ) {
			$t->{class} = $t->{token}->set_class( 'Magic' );
			return 1;
		}

		if ( $char =~ /[\w:]/ ) {
			# Symbol (unless the thing before it is a number
			my ( $prev ) = $t->_previous_significant_tokens(1);
			if ( not $prev or not $prev->isa('PPI::Token::Number') ) {
				$t->{class} = $t->{token}->set_class( 'Symbol' );
				return 1;
			}
		}

		if ( $char eq '{' ) {
			# Get rest of line
			pos $t->{line} = $t->{line_cursor} + 1;
			if ( $t->{line} =~ m/$CURLY_SYMBOL/gc ) {
				# control-character symbol (e.g. %{^_Foo})
				$t->{class} = $t->{token}->set_class( 'Magic' );
				return 1;
			}
		}

		return $self->_as_cast_or_op($t) if $self->_is_cast_or_op($char);

		# Probably the mod operator
		$t->{class} = $t->{token}->set_class( 'Operator' );
		return $t->{class}->__TOKENIZER__on_char( $t );



	} elsif ( $c eq '&' ) {
		# Postfix dereference: ->&*
		if ( $char eq '*' ) {
			my ( $prev ) = $t->_previous_significant_tokens(1);
			if ( $prev and $prev->isa('PPI::Token::Operator') and $prev->content eq '->' ) {
				$t->{class} = $t->{token}->set_class( 'Cast' );
				return 1;
			}
		}

		# Is it a number?
		if ( $char =~ /\d/ ) {
			# bitwise operator
			$t->{class} = $t->{token}->set_class( 'Operator' );
			return $t->_finalize_token->__TOKENIZER__on_char( $t );
		}

		if ( $char =~ /[\w:]/ ) {
			# Symbol (unless the thing before it is a number
			my ( $prev ) = $t->_previous_significant_tokens(1);
			if ( not $prev or not $prev->isa('PPI::Token::Number') ) {
				$t->{class} = $t->{token}->set_class( 'Symbol' );
				return 1;
			}
		}

		return $self->_as_cast_or_op($t) if $self->_is_cast_or_op($char);

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
		if ( $self->__TOKENIZER__is_an_attribute( $t ) ) {
			# This : is an attribute indicator
			$t->{class} = $t->{token}->set_class( 'Operator' );
			$t->{token}->{_attribute} = 1;
			return $t->_finalize_token->__TOKENIZER__on_char( $t );
		}

		# It MIGHT be a label, but it's probably the ?: trinary operator
		$t->{class} = $t->{token}->set_class( 'Operator' );
		return $t->{class}->__TOKENIZER__on_char( $t );
	}

	# erm...
	PPI::Exception->throw('Unknown value in PPI::Token::Unknown token');
}

sub _is_cast_or_op {
	my ( $self, $char ) = @_;
	return 1 if $char eq '$';
	return 1 if $char eq '@';
	return 1 if $char eq '%';
	return 1 if $char eq '*';
	return 1 if $char eq '{';
	return;
}

sub _as_cast_or_op {
	my ( $self, $t ) = @_;
	my $class = _cast_or_op( $t );
	$t->{class} = $t->{token}->set_class( $class );
	return $t->_finalize_token->__TOKENIZER__on_char( $t );
}

sub _prev_significant_w_cursor {
	my ( $tokens, $cursor, $extra_check ) = @_;
	while ( $cursor >= 0 ) {
		my $token = $tokens->[ $cursor-- ];
		next if !$token->significant;
		next if $extra_check and !$extra_check->($token);
		return ( $token, $cursor );
	}
	return ( undef, $cursor );
}

# Operator/operand-sensitive, multiple or GLOB cast
sub _cast_or_op {
	my ( $t ) = @_;

	my $tokens = $t->{tokens};
	my $cursor = scalar( @$tokens ) - 1;
	my $token;

	( $token, $cursor ) = _prev_significant_w_cursor( $tokens, $cursor );
	return 'Cast' if !$token;    # token was first in the document

	if ( $token->isa( 'PPI::Token::Structure' ) and $token->content eq '}' ) {

		# Scan the token stream backwards an arbitrarily long way,
		# looking for the matching opening curly brace.
		my $structure_depth = 1;
		( $token, $cursor ) = _prev_significant_w_cursor(
			$tokens, $cursor,
			sub {
				my ( $token ) = @_;
				return if !$token->isa( 'PPI::Token::Structure' );
				if ( $token eq '}' ) {
					$structure_depth++;
					return;
				}
				if ( $token eq '{' ) {
					$structure_depth--;
					return if $structure_depth;
				}
				return 1;
			}
		);
		return 'Operator' if !$token;    # no matching '{', probably an unbalanced '}'

		# Scan past any whitespace
		( $token, $cursor ) = _prev_significant_w_cursor( $tokens, $cursor );
		return 'Operator' if !$token;                             # Document began with what must be a hash constructor.
		return 'Operator' if $token->isa( 'PPI::Token::Symbol' ); # subscript

		my %meth_or_subscript_end = map { $_ => 1 } qw@ -> } ] @;
		return 'Operator' if $meth_or_subscript_end{ $token->content };    # subscript

		my $content = $token->content;
		my $produces_or_wants_value =
		  ( $token->isa( 'PPI::Token::Word' ) and ( $content eq 'do' or $content eq 'eval' ) );
		return $produces_or_wants_value ? 'Operator' : 'Cast';
	}

	my %list_start_or_term_end = map { $_ => 1 } qw@ ; ( { [ @;
	return 'Cast'
	  if $token->isa( 'PPI::Token::Structure' ) and $list_start_or_term_end{ $token->content }
	  or $token->isa( 'PPI::Token::Cast' )
	  or $token->isa( 'PPI::Token::Operator' )
	  or $token->isa( 'PPI::Token::Label' );

	return 'Operator' if !$token->isa( 'PPI::Token::Word' );

	( $token, $cursor ) = _prev_significant_w_cursor( $tokens, $cursor );
	return 'Cast' if !$token || $token->content ne '->';

	return 'Operator';
}

# Are we at a location where a ':' would indicate a subroutine attribute
sub __TOKENIZER__is_an_attribute {
	my $t      = $_[1]; # Tokenizer object
	my @tokens = $t->_previous_significant_tokens(3);
	my $p0     = $tokens[0];
	return '' if not $p0;

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
	my $p1 = $tokens[1];
	my $p2 = $tokens[2];
	if (
		$p1
		and
		$p1->isa('PPI::Token::Word')
		and
		$p1->content eq 'sub'
		and (
			not $p2
			or
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

	# We aren't an attribute
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
