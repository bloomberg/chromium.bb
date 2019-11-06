package PPI::Token::Number::Exp;

=pod

=head1 NAME

PPI::Token::Number::Exp - Token class for an exponential notation number

=head1 SYNOPSIS

  $n = 1.0e-2;
  $n = 1e+2;

=head1 INHERITANCE

  PPI::Token::Number::Exp
  isa PPI::Token::Number::Float
      isa PPI::Token::Number
          isa PPI::Token
              isa PPI::Element

=head1 DESCRIPTION

The C<PPI::Token::Number::Exp> class is used for tokens that
represent floating point numbers with exponential notation.

=head1 METHODS

=cut

use strict;
use PPI::Token::Number::Float ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token::Number::Float';
}

=pod

=head2 literal

Return the numeric value of this token.

=cut

sub literal {
	my $self = shift;
	return if $self->{_error};
	my ($mantissa, $exponent) = split m/e/i, $self->_literal;
	my $neg = $mantissa =~ s/^\-//;
	$mantissa =~ s/^\./0./;
	$exponent =~ s/^\+//;
	my $val = $mantissa * 10 ** $exponent;
	return $neg ? -$val : $val;
}





#####################################################################
# Tokenizer Methods

sub __TOKENIZER__on_char {
	my $class = shift;
	my $t     = shift;
	my $char  = substr( $t->{line}, $t->{line_cursor}, 1 );

        # To get here, the token must have already encountered an 'E'

	# Allow underscores straight through
	return 1 if $char eq '_';

	# Allow digits
	return 1 if $char =~ /\d/o;

	# Start of exponent is special
	if ( $t->{token}->{content} =~ /e$/i ) {
		# Allow leading +/- in exponent
		return 1 if $char eq '-' || $char eq '+';

		# Invalid character in exponent.  Recover
		if ( $t->{token}->{content} =~ s/\.(e)$//i ) {
			my $word = $1;
			$t->{class} = $t->{token}->set_class('Number');
			$t->_new_token('Operator', '.');
			$t->_new_token('Word', $word);
			return $t->{class}->__TOKENIZER__on_char( $t );
		}
		else {
			$t->{token}->{_error} = "Illegal character in exponent '$char'";
		}
	}

	# Doesn't fit a special case, or is after the end of the token
	# End of token.
	$t->_finalize_token->__TOKENIZER__on_char( $t );
}

1;

=pod

=head1 SUPPORT

See the L<support section|PPI/SUPPORT> in the main module.

=head1 AUTHOR

Chris Dolan E<lt>cdolan@cpan.orgE<gt>

=head1 COPYRIGHT

Copyright 2006 Chris Dolan.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
