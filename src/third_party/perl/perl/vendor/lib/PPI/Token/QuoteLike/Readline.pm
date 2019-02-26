package PPI::Token::QuoteLike::Readline;

=pod

=head1 NAME

PPI::Token::QuoteLike::Readline - The readline quote-like operator

=head1 INHERITANCE

  PPI::Token::QuoteLike::Readline
  isa PPI::Token::QuoteLike
      isa PPI::Token
          isa PPI::Element

=head1 DESCRIPTION

The C<readline> quote-like operator is used to read either a single
line from a file, or all the lines from a file, as follows.

  # Read in a single line
  $line = <FILE>;
  
  # From a scalar handle
  $line = <$filehandle>;
  
  # Read all the lines
  @lines = <FILE>;

=head1 METHODS

There are no methods available for C<PPI::Token::QuoteLike::Readline>
beyond those provided by the parent L<PPI::Token::QuoteLike>, L<PPI::Token>
and L<PPI::Element> classes.

Got any ideas for methods? Submit a report to rt.cpan.org!

=cut

use strict;
use PPI::Token::QuoteLike          ();
use PPI::Token::_QuoteEngine::Full ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = qw{
		PPI::Token::_QuoteEngine::Full
		PPI::Token::QuoteLike
	};
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
