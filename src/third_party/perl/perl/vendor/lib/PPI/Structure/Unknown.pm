package PPI::Structure::Unknown;

=pod

=head1 NAME

PPI::Structure::Unknown - An unknown or unresolved brace structure

=head1 INHERITANCE

  PPI::Structure::Unknown
  isa PPI::Structure
      isa PPI::Node
          isa PPI::Element

=head1 DESCRIPTION

C<PPI::Structure::Unknown> is class for braces whose type is unknown, or
temporarily unknown.

It primarily exists temporarily inside the lexer. Although some types of
braces can be determined immediately at opening, there are a number of
different brace types that can only be correctly identified after the
braces are closed.

A structure is typed as unknown during this period it is indeterminate.

A C<PPI::Structure::Unknown> object should not B<ever> make it out of the
lexer without being converted to it's final type. Any time you encounter
this class in a PDOM tree it should be considered a bug and reported
accordingly.

=head1 METHODS

C<PPI::Structure::Unknown> has no methods beyond those provided by the
standard L<PPI::Structure>, L<PPI::Node> and L<PPI::Element> methods.

Got any ideas for methods? Submit a report to rt.cpan.org!

=cut

use strict;
use PPI::Structure ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Structure';
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
