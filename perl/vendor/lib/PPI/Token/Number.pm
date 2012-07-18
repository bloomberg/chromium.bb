package PPI::Token::Number;

=pod

=head1 NAME

PPI::Token::Number - Token class for a number

=head1 SYNOPSIS

  $n = 1234;       # decimal integer
  $n = 0b1110011;  # binary integer
  $n = 01234;      # octal integer
  $n = 0x1234;     # hexadecimal integer
  $n = 12.34e-56;  # exponential notation ( currently not working )

=head1 INHERITANCE

  PPI::Token::Number
  isa PPI::Token
      isa PPI::Element

=head1 DESCRIPTION

The C<PPI::Token::Number> class is used for tokens that represent numbers,
in the various types that Perl supports.

=head1 METHODS

=cut

use strict;
use PPI::Token ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token';
}

=pod

=head2 base

The C<base> method is provided by all of the ::Number subclasses.
This is 10 for decimal, 16 for hexadecimal, 2 for binary, etc.

=cut

sub base {
	return 10;
}

=pod

=head2 literal

Return the numeric value of this token.

=cut

sub literal {
	return 0 + $_[0]->_literal;
}

sub _literal {
	# De-sugar the string representation
	my $self   = shift;
	my $string = $self->content;
	$string =~ s/^\+//;
	$string =~ s/_//g;
	return $string;
}





#####################################################################
# Tokenizer Methods

sub __TOKENIZER__on_char {
	my $class = shift;
	my $t     = shift;
	my $char  = substr( $t->{line}, $t->{line_cursor}, 1 );

	# Allow underscores straight through
	return 1 if $char eq '_';

	# Handle the conversion from an unknown to known type.
	# The regex covers "potential" hex/bin/octal number.
	my $token = $t->{token};
	if ( $token->{content} =~ /^-?0_*$/ ) {
		# This could be special
		if ( $char eq 'x' ) {
			$t->{class} = $t->{token}->set_class( 'Number::Hex' );
			return 1;
		} elsif ( $char eq 'b' ) {
			$t->{class} = $t->{token}->set_class( 'Number::Binary' );
			return 1;
		} elsif ( $char =~ /\d/ ) {
			# You cannot have 8s and 9s on octals
			if ( $char eq '8' or $char eq '9' ) {
				$token->{_error} = "Illegal character in octal number '$char'";
			}
			$t->{class} = $t->{token}->set_class( 'Number::Octal' );
			return 1;
		}
	}

	# Handle the easy case, integer or real.
	return 1 if $char =~ /\d/o;

	if ( $char eq '.' ) {
		$t->{class} = $t->{token}->set_class( 'Number::Float' );
		return 1;
	}
	if ( $char eq 'e' || $char eq 'E' ) {
		$t->{class} = $t->{token}->set_class( 'Number::Exp' );
		return 1;
	}

	# Doesn't fit a special case, or is after the end of the token
	# End of token.
	$t->_finalize_token->__TOKENIZER__on_char( $t );
}

1;

=pod

=head1 CAVEATS

Compared to Perl, the number tokenizer is too liberal about allowing
underscores anywhere.  For example, the following is a syntax error in
Perl, but is allowed in PPI:

   0_b10

=head1 TO DO

- Treat v-strings as binary strings or barewords, not as "base-256"
  numbers

- Break out decimal integers into their own subclass?

- Implement literal()

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
