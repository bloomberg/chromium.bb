package PPI::Token::Number::Hex;

=pod

=head1 NAME

PPI::Token::Number::Hex - Token class for a binary number

=head1 SYNOPSIS

  $n = 0x1234;     # hexadecimal integer

=head1 INHERITANCE

  PPI::Token::Number::Hex
  isa PPI::Token::Number
      isa PPI::Token
          isa PPI::Element

=head1 DESCRIPTION

The C<PPI::Token::Number::Hex> class is used for tokens that
represent base-16 numbers.

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

Returns the base for the number: 16.

=cut

sub base () { 16 }

=pod

=head2 literal

Return the numeric value of this token.

=cut

sub literal {
	my $self = shift;
	my $str = $self->_literal;
	my $neg = $str =~ s/^\-//;
	my $val = hex $str;
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

	if ( $char =~ /[\da-f]/ ) {
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
