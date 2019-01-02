package PPI::Token::Operator;

=pod

=head1 NAME

PPI::Token::Operator - Token class for operators

=head1 INHERITANCE

  PPI::Token::Operator
  isa PPI::Token
      isa PPI::Element

=head1 SYNOPSIS

  # This is the list of valid operators
  ++   --   **   !    ~    +    -
  =~   !~   *    /    %    x
  <<   >>   lt   gt   le   ge   cmp  ~~
  ==   !=   <=>  .    ..   ...  ,
  &    |    ^    &&   ||   //
  ?    :    =    +=   -=   *=   .=   //=
  <    >    <=   >=   <>   =>   ->
  and  or   dor  not  eq   ne

=head1 DESCRIPTION

All operators in PPI are created as C<PPI::Token::Operator> objects,
including the ones that may superficially look like a L<PPI::Token::Word>
object.

=head1 METHODS

There are no additional methods beyond those provided by the parent
L<PPI::Token> and L<PPI::Element> classes.

Got any ideas for methods? Submit a report to rt.cpan.org!

=cut

use strict;
use PPI::Token ();

use vars qw{$VERSION @ISA %OPERATOR};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token';

	# Build the operator index
	### NOTE - This is accessed several times explicitly
	###        in PPI::Token::Word. Do not rename this
	###        without also correcting them.
	%OPERATOR = map { $_ => 1 } (
		qw{
		-> ++ -- ** ! ~ + -
		=~ !~ * / % x . << >>
		< > <= >= lt gt le ge
		== != <=> eq ne cmp ~~
		& | ^ && || // .. ...
		? : = += -= *= .= /= //=
		=> <>
		and or xor not
		}, ',' 	# Avoids "comma in qw{}" warning
		);
}





#####################################################################
# Tokenizer Methods

sub __TOKENIZER__on_char {
	my $t    = $_[1];
	my $char = substr( $t->{line}, $t->{line_cursor}, 1 );

	# Are we still an operator if we add the next character
	my $content = $t->{token}->{content};
	return 1 if $OPERATOR{ $content . $char };

	# Handle the special case of a .1234 decimal number
	if ( $content eq '.' ) {
		if ( $char =~ /^[0-9]$/ ) {
			# This is a decimal number
			$t->{class} = $t->{token}->set_class('Number::Float');
			return $t->{class}->__TOKENIZER__on_char( $t );
		}
	}

	# Handle the special case if we might be a here-doc
	if ( $content eq '<<' ) {
		my $line = substr( $t->{line}, $t->{line_cursor} );
		# Either <<FOO or << 'FOO' or <<\FOO
		### Is the zero-width look-ahead assertion really
		### supposed to be there?
		if ( $line =~ /^(?: (?!\d)\w | \s*['"`] | \\\w ) /x ) {
			# This is a here-doc.
			# Change the class and move to the HereDoc's own __TOKENIZER__on_char method.
			$t->{class} = $t->{token}->set_class('HereDoc');
			return $t->{class}->__TOKENIZER__on_char( $t );
		}
	}

	# Handle the special case of the null Readline
	if ( $content eq '<>' ) {
		$t->{class} = $t->{token}->set_class('QuoteLike::Readline');
	}

	# Finalize normally
	$t->_finalize_token->__TOKENIZER__on_char( $t );
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
