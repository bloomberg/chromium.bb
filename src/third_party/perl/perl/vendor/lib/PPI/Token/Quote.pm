package PPI::Token::Quote;

=pod

=head1 NAME

PPI::Token::Quote - String quote abstract base class

=head1 INHERITANCE

  PPI::Token::Quote
  isa PPI::Token
      isa PPI::Element

=head1 DESCRIPTION

The C<PPI::Token::Quote> class is never instantiated, and simply
provides a common abstract base class for the four quote classes.
In PPI, a "quote" is limited to only the quote-like things that
themselves directly represent a string. (although this includes
double quotes with interpolated elements inside them).

The subclasses of C<PPI::Token::Quote> are:

=over 2

=item C<''> - L<PPI::Token::Quote::Single>

=item C<q{}> - L<PPI::Token::Quote::Literal>

=item C<""> - L<PPI::Token::Quote::Double>

=item C<qq{}> - L<PPI::Token::Quote::Interpolate>

=back

The names are hopefully obvious enough not to have to explain what
each class is here. See their respective pages for more details.

Please note that although the here-doc B<does> represent a literal
string, it is such a nasty piece of work that in L<PPI> it is given the
honor of its own token class (L<PPI::Token::HereDoc>).

=head1 METHODS

=cut

use strict;
use PPI::Token ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token';
}





#####################################################################
# PPI::Token::Quote Methods

=pod

=head2 string

The C<string> method is provided by all four ::Quote classes. It won't
get you the actual literal Perl value, but it will strip off the wrapping
of the quotes.

  # The following all return foo from the ->string method
  'foo'
  "foo"
  q{foo}
  qq <foo>

=begin testing string 15

# Prove what we say in the ->string docs
my $Document = PPI::Document->new(\<<'END_PERL');
  'foo'
  "foo"
  q{foo}
  qq <foo>
END_PERL
isa_ok( $Document, 'PPI::Document' );

my $quotes = $Document->find('Token::Quote');
is( ref($quotes), 'ARRAY', 'Found quotes' );
is( scalar(@$quotes), 4, 'Found 4 quotes' );
foreach my $Quote ( @$quotes ) {
	isa_ok( $Quote, 'PPI::Token::Quote');
	can_ok( $Quote, 'string'           );
	is( $Quote->string, 'foo', '->string returns "foo" for '
		. $Quote->content );
}

=end testing

=cut

#sub string {
#	my $class = ref $_[0] || $_[0];
#	die "$class does not implement method ->string";
#}

=pod

=head2 literal

The C<literal> method is provided by ::Quote:Literal and
::Quote::Single.  This returns the value of the string as Perl sees
it: without the quote marks and with C<\\> and C<\'> resolved to C<\>
and C<'>.

The C<literal> method is not implemented by ::Quote::Double or
::Quote::Interpolate yet.

=cut

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
