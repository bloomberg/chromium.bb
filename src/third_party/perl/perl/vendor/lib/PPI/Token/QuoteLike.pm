package PPI::Token::QuoteLike;

=pod

=head1 NAME

PPI::Token::QuoteLike - Quote-like operator abstract base class

=head1 INHERITANCE

  PPI::Token::QuoteLike
  isa PPI::Token
      isa PPI::Element

=head1 DESCRIPTION

The C<PPI::Token::QuoteLike> class is never instantiated, and simply
provides a common abstract base class for the five quote-like operator
classes. In PPI, a "quote-like" is the set of quote-like things that
exclude the string quotes and regular expressions.

The subclasses of C<PPI::Token::QuoteLike> are:

=over 2

=item qw{} - L<PPI::Token::QuoteLike::Words>

=item `` - L<PPI::Token::QuoteLike::Backtick>

=item qx{} - L<PPI::Token::QuoteLike::Command>

=item qr// - L<PPI::Token::QuoteLike::Regexp>

=item <FOO> - L<PPI::Token::QuoteLike::Readline>

=back

The names are hopefully obvious enough not to have to explain what
each class is. See their pages for more details.

You may note that the backtick and command quote-like are treated
separately, even though they do the same thing. This is intentional,
as the inherit from and are processed by two different parts of the
PPI's quote engine.

=cut

use strict;
use PPI::Token ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token';
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
