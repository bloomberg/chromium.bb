package PPI::Statement::Break;

=pod

=head1 NAME

PPI::Statement::Break - Statements which break out of normal statement flow

=head1 SYNOPSIS

  last;
  goto FOO;
  next if condition();
  return $foo;
  redo;

=head1 INHERITANCE

  PPI::Statement::Break
  isa PPI::Statement
      isa PPI::Node
          isa PPI::Element

=head1 DESCRIPTION

C<PPI::Statement::Break> is intended to represent statements that break
out of the normal statement flow control. This covers the basic
types C<'redo'>, C<'goto'>, C<'next'>, C<'last'> and C<'return'>.

=head1 METHODS

C<PPI::Statement::Break> has no additional methods beyond the default ones
provided by L<PPI::Statement>, L<PPI::Node> and L<PPI::Element>.

However, it is expected to gain methods for identifying the line to break
to, or the structure to break out of.

=cut

use strict;
use PPI::Statement ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Statement';
}

1;

=pod

=head1 TO DO

- Add the methods to identify the break target

- Add some proper unit testing

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
