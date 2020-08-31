package PPI::Token::Quote::Interpolate;

=pod

=head1 NAME

PPI::Token::Quote::Interpolate - The interpolation quote-like operator

=head1 INHERITANCE

  PPI::Token::Quote::Interpolate
  isa PPI::Token::Quote
      isa PPI::Token
          isa PPI::Element

=head1 DESCRIPTION

A C<PPI::Token::Quote::Interpolate> object represents a single
interpolation quote-like operator, such as C<qq{$foo bar $baz}>.

=head1 METHODS

There are no methods available for C<PPI::Token::Quote::Interpolate>
beyond those provided by the parent L<PPI::Token::Quote>, L<PPI::Token> and
L<PPI::Element> classes.

=cut

use strict;
use PPI::Token::Quote ();
use PPI::Token::_QuoteEngine::Full ();

our $VERSION = '1.269'; # VERSION

our @ISA = qw{
	PPI::Token::_QuoteEngine::Full
	PPI::Token::Quote
};





#####################################################################
# PPI::Token::Quote Methods

sub string {
	my $self     = shift;
	my @sections = $self->_sections;
	my $str      = $sections[0];
	substr( $self->{content}, $str->{position}, $str->{size} );	
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
