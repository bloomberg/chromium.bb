package PPI::Statement::Variable;

=pod

=head1 NAME

PPI::Statement::Variable - Variable declaration statements

=head1 SYNOPSIS

  # All of the following are variable declarations
  my $foo = 1;
  my ($foo, $bar) = (1, 2);
  our $foo = 1;
  local $foo;
  local $foo = 1;
  LABEL: my $foo = 1;

=head1 INHERITANCE

  PPI::Statement::Variable
  isa PPI::Statement::Expression
      isa PPI::Statement
          isa PPI::Node
              isa PPI::Element

=head1 DESCRIPTION

The main intent of the C<PPI::Statement::Variable> class is to describe
simple statements that explicitly declare new local or global variables.

Note that this does not make it exclusively the only place where variables
are defined, and later on you should expect that the C<variables> method
will migrate deeper down the tree to either L<PPI::Statement> or
L<PPI::Node> to recognise this fact, but for now it stays here.

=head1 METHODS

=cut

use strict;
use Params::Util               qw{_INSTANCE};
use PPI::Statement::Expression ();

our $VERSION = '1.269'; # VERSION

our @ISA = "PPI::Statement::Expression";

=pod

=head2 type

The C<type> method checks and returns the declaration type of the statement,
which will be one of 'my', 'local', 'our', or 'state'.

Returns a string of the type, or C<undef> if the type cannot be detected
(which is probably a bug).

=cut

sub type {
	my $self = shift;

	# Get the first significant child
	my @schild = grep { $_->significant } $self->children;

	# Ignore labels
	shift @schild if _INSTANCE($schild[0], 'PPI::Token::Label');

	# Get the type
	(_INSTANCE($schild[0], 'PPI::Token::Word') and $schild[0]->content =~ /^(my|local|our|state)$/)
		? $schild[0]->content
		: undef;
}

=pod

=head2 variables

As for several other PDOM Element types that can declare variables, the
C<variables> method returns a list of the canonical forms of the variables
defined by the statement.

Returns a list of the canonical string forms of variables, or the null list
if it is unable to find any variables.

=cut

sub variables {
	map { $_->canonical } $_[0]->symbols;
}

=pod

=head2 symbols

Returns a list of the variables defined by the statement, as
L<PPI::Token::Symbol>s.

=cut

sub symbols {
	my $self = shift;

	# Get the children we care about
	my @schild = grep { $_->significant } $self->children;
	shift @schild if _INSTANCE($schild[0], 'PPI::Token::Label');

	# If the second child is a symbol, return its name
	if ( _INSTANCE($schild[1], 'PPI::Token::Symbol') ) {
		return $schild[1];
	}

	# If it's a list, return as a list
	if ( _INSTANCE($schild[1], 'PPI::Structure::List') ) {
		my $Expression = $schild[1]->schild(0);
		$Expression and
		$Expression->isa('PPI::Statement::Expression') or return ();

		# my and our are simpler than local
		if (
			$self->type eq 'my'
			or
			$self->type eq 'our'
			or
			$self->type eq 'state'
		) {
			return grep {
				$_->isa('PPI::Token::Symbol')
			} $Expression->schildren;
		}

		# Local is much more icky (potentially).
		# Not that we are actually going to deal with it now,
		# but having this separate is likely going to be needed
		# for future bug reports about local() things.

		# This is a slightly better way to check.
		return grep {
			$self->_local_variable($_)
		} grep {
			$_->isa('PPI::Token::Symbol')
		} $Expression->schildren;
	}

	# erm... this is unexpected
	();
}

sub _local_variable {
	my ($self, $el) = @_;

	# The last symbol should be a variable
	my $n = $el->snext_sibling or return 1;
	my $p = $el->sprevious_sibling;
	if ( ! $p or $p eq ',' ) {
		# In the middle of a list
		return 1 if $n eq ',';

		# The first half of an assignment
		return 1 if $n eq '=';
	}

	# Lets say no for know... additional work
	# should go here.
	return '';
}

1;

=pod

=head1 TO DO

- Write unit tests for this

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
