package PPI::Structure::When;

=pod

=head1 NAME

PPI::Structure::When - Circular braces for a when statement

=head1 SYNOPSIS

  when ( something ) {
      ...
  }

=head1 INHERITANCE

  PPI::Structure::When
  isa PPI::Structure
      isa PPI::Node
          isa PPI::Element

=head1 DESCRIPTION

C<PPI::Structure::When> is the class used for circular braces that
contain the thing to be matched in a when statement.

=head1 METHODS

C<PPI::Structure::When> has no methods beyond those provided by the
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
