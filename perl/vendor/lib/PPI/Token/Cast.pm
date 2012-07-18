package PPI::Token::Cast;

=pod

=head1 NAME

PPI::Token::Cast - A prefix which forces a value into a different context

=head1 INHERITANCE

  PPI::Token::Cast
  isa PPI::Token
      isa PPI::Element

=head1 DESCRIPTION

A "cast" in PPI terms is one of more characters used as a prefix which force
a value into a different class or context.

This includes referencing, dereferencing, and a few other minor cases.

For expressions such as C<@$foo> or C<@{ $foo{bar} }> the C<@> in both cases
represents a cast. In this case, an array dereference.

=head1 METHODS

There are no additional methods beyond those provided by the parent
L<PPI::Token> and L<PPI::Element> classes.

Got any ideas for methods? Submit a report to rt.cpan.org!

=cut

use strict;
use PPI::Token ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token';
}




#####################################################################
# Tokenizer Methods

# A cast is either % @ $ or $#
sub __TOKENIZER__on_char {
	$_[1]->_finalize_token->__TOKENIZER__on_char( $_[1] );
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
