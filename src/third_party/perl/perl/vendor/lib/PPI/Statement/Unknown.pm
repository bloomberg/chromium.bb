package PPI::Statement::Unknown;

=pod

=head1 NAME

PPI::Statement::Unknown - An unknown or transient statement

=head1 INHERITANCE

  PPI::Statement::Unknown
  isa PPI::Statement
      isa PPI::Node
          isa PPI::Element

=head1 DESCRIPTION

The C<PPI::Statement::Unknown> class is used primarily during the lexing
process to hold elements that are known to be statement, but for which
the exact C<type> of statement is as yet unknown, and requires further
tokens in order to resolve the correct type.

They should not exist in a fully parse B<valid> document, and if any
exists they indicate either a problem in Document, or possibly (by
allowing it to get through unresolved) a bug in L<PPI::Lexer>.

=head1 METHODS

C<PPI::Statement::Unknown> has no additional methods beyond the
default ones provided by L<PPI::Statement>, L<PPI::Node> and
L<PPI::Element>.

=cut

use strict;
use PPI::Statement ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Statement';
}

# If one of these ends up in the final document,
# we're pretty much screwed. Just call it a day.
sub _complete () { 1 }

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
