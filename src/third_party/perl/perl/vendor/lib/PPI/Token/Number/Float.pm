package PPI::Token::Number::Float;

=pod

=head1 NAME

PPI::Token::Number::Float - Token class for a floating-point number

=head1 SYNOPSIS

  $n = 1.234;

=head1 INHERITANCE

  PPI::Token::Number::Float
  isa PPI::Token::Number
      isa PPI::Token
          isa PPI::Element

=head1 DESCRIPTION

The C<PPI::Token::Number::Float> class is used for tokens that
represent floating point numbers.  A float is identified by n decimal
point.  Exponential notation (the C<e> or C<E>) is handled by the
PPI::Token::Number::Exp class.

=head1 METHODS

=cut

use strict;
use PPI::Token::Number ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token::Number';
}

=pod

=head2 base

Returns the base for the number: 10.

=cut

sub base () { 10 }

=pod

=head2 literal

Return the numeric value of this token.

=cut

sub literal {
	my $self = shift;
	my $str = $self->_literal;
	my $neg = $str =~ s/^\-//;
	$str =~ s/^\./0./;
	my $val = 0+$str;
	return $neg ? -$val : $val;
}





#####################################################################
# Tokenizer Methods

sub __TOKENIZER__on_char {
	my $class = shift;
	my $t     = shift;
	my $char  = substr( $t->{line}, $t->{line_cursor}, 1 );

	# Allow underscores straight through
	return 1 if $char eq '_';

	# Allow digits
	return 1 if $char =~ /\d/o;

	# Is there a second decimal point?  Then version string or '..' operator
	if ( $char eq '.' ) {
		if ( $t->{token}->{content} =~ /\.$/ ) {
			# We have a .., which is an operator.
			# Take the . off the end of the token..
			# and finish it, then make the .. operator.
			chop $t->{token}->{content};
                        $t->{class} = $t->{token}->set_class( 'Number' );
			$t->_new_token('Operator', '..');
			return 0;
		} elsif ( $t->{token}->{content} !~ /_/ ) {
			# Underscore means not a Version, fall through to end token
			$t->{class} = $t->{token}->set_class( 'Number::Version' );
			return 1;
		}
	}
	if ($char eq 'e' || $char eq 'E') {
		$t->{class} = $t->{token}->set_class( 'Number::Exp' );
		return 1;
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
