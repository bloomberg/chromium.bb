package PPI::Token::Number::Binary;

=pod

=head1 NAME

PPI::Token::Number::Binary - Token class for a binary number

=head1 SYNOPSIS

  $n = 0b1110011;  # binary integer

=head1 INHERITANCE

  PPI::Token::Number::Binary
  isa PPI::Token::Number
      isa PPI::Token
          isa PPI::Element

=head1 DESCRIPTION

The C<PPI::Token::Number::Binary> class is used for tokens that
represent base-2 numbers.

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

Returns the base for the number: 2.

=cut

sub base {
	return 2;
}

=pod

=head2 literal

Return the numeric value of this token.

=cut

sub literal {
	my $self = shift;
	return if $self->{_error};
	my $str = $self->_literal;
	my $neg = $str =~ s/^\-//;
	$str =~ s/^0b//;
	my $val = 0;
	for my $bit ( $str =~ m/(.)/g ) {
		$val = $val * 2 + $bit;
	}
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

	if ( $char =~ /[\w\d]/ ) {
		unless ( $char eq '1' or $char eq '0' ) {
			# Add a warning if it contains non-hex chars
			$t->{token}->{_error} = "Illegal character in binary number '$char'";
		}
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
