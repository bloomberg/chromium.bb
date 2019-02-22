package PPI::Statement::Data;

=pod

=head1 NAME

PPI::Statement::Data - The __DATA__ section of a file

=head1 SYNOPSIS

  # Normal content
  
  __DATA__
  This: data
  is: part
  of: the
  PPI::Statement::Data: object

=head1 INHERITANCE

  PPI::Statement::Compound
  isa PPI::Statement
      isa PPI::Node
          isa PPI::Element

=head1 DESCRIPTION

C<PPI::Statement::Data> is a utility class designed to hold content in
the __DATA__ section of a file. It provides a single statement to hold
B<all> of the data.

=head1 METHODS

C<PPI::Statement::Data> has no additional methods beyond the default ones
provided by L<PPI::Statement>, L<PPI::Node> and L<PPI::Element>.

However, it is expected to gain methods for accessing the data directly,
(as a filehandle for example) just as you would access the data in the
Perl code itself.

=cut

use strict;
use PPI::Statement ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Statement';
}

# Data is never complete
sub _complete () { '' }

1;

=pod

=head1 TO DO

- Add the methods to read in the data

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
