package PPI::Statement::Null;

=pod

=head1 NAME

PPI::Statement::Null - A useless null statement

=head1 SYNOPSIS

  my $foo = 1;
  
  ; # <-- Null statement
  
  my $bar = 1;

=head1 INHERITANCE

  PPI::Statement::Null
  isa PPI::Statement
      isa PPI::Node
          isa PPI::Element

=head1 DESCRIPTION

C<PPI::Statement::Null> is a utility class designed to handle situations
where PPI encounters a naked statement separator.

Although strictly speaking, the semicolon is a statement B<separator>
and not a statement B<terminator>, PPI considers a semicolon to be a
statement terminator under most circumstances.

In any case, the null statement has no purpose, and can be safely deleted
with no ill effect.

=head1 METHODS

C<PPI::Statement::Null> has no additional methods beyond the default ones
provided by L<PPI::Statement>, L<PPI::Node> and L<PPI::Element>.

=cut

use strict;
use PPI::Statement ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Statement';
}

# A null statement is not significant
sub significant { '' }

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
