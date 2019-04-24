package PPI::Statement::End;

=pod

=head1 NAME

PPI::Statement::End - Content after the __END__ of a module

=head1 SYNOPSIS

  # This is normal content
  
  __END__
  
  This is part of an PPI::Statement::End statement
  
  =pod
  
  This is not part of the ::End statement, it's POD
  
  =cut
  
  This is another PPI::Statement::End statement

=head1 INHERITANCE

  PPI::Statement::End
  isa PPI::Statement
      isa PPI::Node
          isa PPI::Element

=head1 DESCRIPTION

C<PPI::Statement::End> is a utility class designed to serve as a contained
for all of the content after the __END__ tag in a file.

It doesn't cover the ENTIRE of the __END__ section, and can be interspersed
with L<PPI::Token::Pod> tokens.

=head1 METHODS

C<PPI::Statement::End> has no additional methods beyond the default ones
provided by L<PPI::Statement>, L<PPI::Node> and L<PPI::Element>.

=cut

use strict;
use PPI::Statement ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Statement';
}

# Once we have an __END__ we're done
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
