package PPI::Statement::Compound;

=pod

=head1 NAME

PPI::Statement::Compound - Describes all compound statements

=head1 SYNOPSIS

  # A compound if statement
  if ( foo ) {
      bar();
  } else {
      baz();
  }

  # A compound loop statement
  foreach ( @list ) {
      bar($_);
  }

=head1 INHERITANCE

  PPI::Statement::Compound
  isa PPI::Statement
      isa PPI::Node
          isa PPI::Element

=head1 DESCRIPTION

C<PPI::Statement::Compound> objects are used to describe all current forms
of compound statements, as described in L<perlsyn>.

This covers blocks using C<if>, C<unless>, C<for>, C<foreach>, C<while>,
and C<continue>. Please note this does B<not> cover "simple" statements
with trailing conditions. Please note also that "do" is also not part of
a compound statement.

  # This is NOT a compound statement
  my $foo = 1 if $condition;

  # This is also not a compound statement
  do { ... } until $condition;

=head1 METHODS

C<PPI::Statement::Compound> has a number of methods in addition to the
standard L<PPI::Statement>, L<PPI::Node> and L<PPI::Element> methods.

=cut

use strict;
use PPI::Statement ();

our $VERSION = '1.269'; # VERSION

our @ISA = "PPI::Statement";

# Keyword type map
my %TYPES = (
	'if'      => 'if',
	'unless'  => 'if',
	'while'   => 'while',
	'until'   => 'while',
	'for'     => 'for',
	'foreach' => 'foreach',
);

# Lexer clues
sub __LEXER__normal() { '' }





#####################################################################
# PPI::Statement::Compound analysis methods

=pod

=head2 type

The C<type> method returns the syntactic type of the compound statement.

There are four basic compound statement types.

The C<'if'> type includes all variations of the if and unless statements,
including any C<'elsif'> or C<'else'> parts of the compound statement.

The C<'while'> type describes the standard while and until statements, but
again does B<not> describes simple statements with a trailing while.

The C<'for'> type covers the C-style for loops, regardless of whether they
were declared using C<'for'> or C<'foreach'>.

The C<'foreach'> type covers loops that iterate over collections,
regardless of whether they were declared using C<'for'> or C<'foreach'>.

All of the compounds are a variation on one of these four.

Returns the simple string C<'if'>, C<'for'>, C<'foreach'> or C<'while'>,
or C<undef> if the type cannot be determined.

=cut

sub type {
	my $self    = shift;
	my $p       = 0; # Child position
	my $Element = $self->schild($p) or return undef;

	# A labelled statement
	if ( $Element->isa('PPI::Token::Label') ) {
		$Element = $self->schild(++$p) or return 'label';
	}

	# Most simple cases
	my $content = $Element->content;
	if ( $content =~ /^for(?:each)?\z/ ) {
		$Element = $self->schild(++$p) or return $content;
		if ( $Element->isa('PPI::Token') ) {
			return 'foreach' if $Element->content =~ /^my|our|state\z/;
			return 'foreach' if $Element->isa('PPI::Token::Symbol');
			return 'foreach' if $Element->isa('PPI::Token::QuoteLike::Words');
		}
		if ( $Element->isa('PPI::Structure::List') ) {
			return 'foreach';
		}
		return 'for';
	}
	return $TYPES{$content} if $Element->isa('PPI::Token::Word');
	return 'continue'       if $Element->isa('PPI::Structure::Block');

	# Unknown (shouldn't exist?)
	undef;
}





#####################################################################
# PPI::Node Methods

sub scope() { 1 }





#####################################################################
# PPI::Element Methods

sub _complete {
	my $self = shift;
	my $type = $self->type or die "Illegal compound statement type";

	# Check the different types of compound statements
	if ( $type eq 'if' ) {
		# Unless the last significant child is a complete
		# block, it must be incomplete.
		my $child = $self->schild(-1) or return '';
		$child->isa('PPI::Structure') or return '';
		$child->braces eq '{}'        or return '';
		$child->_complete             or return '';

		# It can STILL be
	} elsif ( $type eq 'while' ) {
		die "CODE INCOMPLETE";
	} else {
		die "CODE INCOMPLETE";
	}
}

1;

=pod

=head1 TO DO

- Write unit tests for this package

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
