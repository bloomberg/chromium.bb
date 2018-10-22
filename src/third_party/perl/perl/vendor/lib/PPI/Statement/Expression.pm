package PPI::Statement::Expression;

=pod

=head1 NAME

PPI::Statement::Expression - A generic and non-specialised statement

=head1 SYNOPSIS

  $foo = bar;
  ("Hello World!");
  do_this();

=head1 INHERITANCE

  PPI::Statement::Expression
  isa PPI::Statement
      isa PPI::Node
          isa PPI::Element

=head1 DESCRIPTION

A C<PPI::Statement::Expression> is a normal statement that is evaluated,
may or may not assign, may or may not have side effects, and has no special
or redeeming features whatsoever.

It provides a default for all statements that don't fit into any other
classes.

=head1 METHODS

C<PPI::Statement::Expression> has no additional methods beyond the default ones
provided by L<PPI::Statement>, L<PPI::Node> and L<PPI::Element>.

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
