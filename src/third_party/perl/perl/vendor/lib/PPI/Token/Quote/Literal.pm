package PPI::Token::Quote::Literal;

=pod

=head1 NAME

PPI::Token::Quote::Literal - The literal quote-like operator

=head1 INHERITANCE

  PPI::Token::Quote::Literal
  isa PPI::Token::Quote
      isa PPI::Token
          isa PPI::Element

=head1 DESCRIPTION

A C<PPI::Token::Quote::Literal> object represents a single literal
quote-like operator, such as C<q{foo bar}>.

=head1 METHODS

There are no methods available for C<PPI::Token::Quote::Literal> beyond
those provided by the parent L<PPI::Token::Quote>, L<PPI::Token> and
L<PPI::Element> classes.

Got any ideas for methods? Submit a report to rt.cpan.org!

=cut

use strict;
use PPI::Token::Quote              ();
use PPI::Token::_QuoteEngine::Full ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = qw{
		PPI::Token::_QuoteEngine::Full
		PPI::Token::Quote
	};
}





#####################################################################
# PPI::Token::Quote Methods

=pod

=begin testing string 8

my $Document = PPI::Document->new( \"print q{foo}, q!bar!, q <foo>;" );
isa_ok( $Document, 'PPI::Document' );
my $literal = $Document->find('Token::Quote::Literal');
is( scalar(@$literal), 3, '->find returns three objects' );
isa_ok( $literal->[0], 'PPI::Token::Quote::Literal' );
isa_ok( $literal->[1], 'PPI::Token::Quote::Literal' );
isa_ok( $literal->[2], 'PPI::Token::Quote::Literal' );
is( $literal->[0]->string, 'foo', '->string returns as expected' );
is( $literal->[1]->string, 'bar', '->string returns as expected' );
is( $literal->[2]->string, 'foo', '->string returns as expected' );

=end testing

=cut

sub string {
	my $self     = shift;
	my @sections = $self->_sections;
	my $str      = $sections[0];
	substr( $self->{content}, $str->{position}, $str->{size} );	
}

=pod

=begin testing literal 4

my $Document = PPI::Document->new( \"print q{foo}, q!bar!, q <foo>;" );
isa_ok( $Document, 'PPI::Document' );
my $literal = $Document->find('Token::Quote::Literal');
is( $literal->[0]->literal, 'foo', '->literal returns as expected' );
is( $literal->[1]->literal, 'bar', '->literal returns as expected' );
is( $literal->[2]->literal, 'foo', '->literal returns as expected' );

=end testing

=cut

*literal = *PPI::Token::Quote::Single::literal;

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
