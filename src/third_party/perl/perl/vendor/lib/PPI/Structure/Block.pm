package PPI::Structure::Block;

=pod

=head1 NAME

PPI::Structure::Block - Curly braces representing a code block

=head1 SYNOPSIS

  sub foo { ... }
  
  grep { ... } @list;
  
  if ( condition ) {
      ...
  }
  
  LABEL: {
      ...
  }

=head1 INHERITANCE

  PPI::Structure::Block
  isa PPI::Structure
      isa PPI::Node
          isa PPI::Element

=head1 DESCRIPTION

C<PPI::Structure::Block> is the class used for all curly braces that
represent code blocks. This includes subroutines, compound statements
and any other block braces.

=head1 METHODS

C<PPI::Structure::Block> has no methods beyond those provided by the
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





#####################################################################
# PPI::Element Methods

# This is a scope boundary
sub scope { 1 }

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
