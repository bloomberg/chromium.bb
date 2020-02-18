package PPI::Statement::Include;

=pod

=head1 NAME

PPI::Statement::Include - Statements that include other code

=head1 SYNOPSIS

  # The following are all includes
  use 5.006;
  use strict;
  use My::Module;
  use constant FOO => 'Foo';
  require Foo::Bar;
  require "Foo/Bar.pm";
  require $foo if 1;
  no strict 'refs';

=head1 INHERITANCE

  PPI::Statement::Include
  isa PPI::Statement
      isa PPI::Node
          isa PPI::Element

=head1 DESCRIPTION

Despite its name, the C<PPI::Statement::Include> class covers a number
of different types of statement that cover all statements starting with
C<use>, C<no> and C<require>.

But basically, they cover three situations.

Firstly, a dependency on a particular version of perl (for which the
C<version> method returns true), a pragma (for which the C<pragma> method
returns true), or the loading (and unloading via no) of modules.

=head1 METHODS

C<PPI::Statement::Include> has a number of methods in addition to the standard
L<PPI::Statement>, L<PPI::Node> and L<PPI::Element> methods.

=cut

use strict;
use PPI::Statement                 ();
use PPI::Statement::Include::Perl6 ();

our $VERSION = '1.269'; # VERSION

our @ISA = "PPI::Statement";

=pod

=head2 type

The C<type> method returns the general type of statement (C<'use'>, C<'no'>
or C<'require'>).

Returns the type as a string, or C<undef> if the type cannot be detected.

=cut

sub type {
	my $self    = shift;
	my $keyword = $self->schild(0) or return undef;
	$keyword->isa('PPI::Token::Word') and $keyword->content;
}

=pod

=head2 module

The C<module> method returns the module name specified in any include
statement. This C<includes> pragma names, because pragma are implemented
as modules. (And lets face it, the definition of a pragma can be fuzzy
at the best of times in any case)

This covers all of these...

  use strict;
  use My::Module;
  no strict;
  require My::Module;

...but does not cover any of these...

  use 5.006;
  require 5.005;
  require "explicit/file/name.pl";

Returns the module name as a string, or C<undef> if the include does
not specify a module name.

=cut

sub module {
	my $self = shift;
	my $module = $self->schild(1) or return undef;
	$module->isa('PPI::Token::Word') and $module->content;
}

=pod

=head2 module_version

The C<module_version> method returns the minimum version of the module
required by the statement, if there is one.

=cut

sub module_version {
	my $self     = shift;
	my $argument = $self->schild(3);
	if ( $argument and $argument->isa('PPI::Token::Operator') ) {
		return undef;
	}

	my $version = $self->schild(2) or return undef;
	return undef unless $version->isa('PPI::Token::Number');

	return $version;
}

=pod

=head2 pragma

The C<pragma> method checks for an include statement's use as a
pragma, and returns it if so.

Or at least, it claims to. In practice it's a lot harder to say exactly
what is or isn't a pragma, because the definition is fuzzy.

The C<intent> of a pragma is to modify the way in which the parser works.
This is done though the use of modules that do various types of internals
magic.

For now, PPI assumes that any "module name" that is only a set of
lowercase letters (and perhaps numbers, like C<use utf8;>). This
behaviour is expected to change, most likely to something that knows
the specific names of the various "pragmas".

Returns the name of the pragma, or false ('') if the include is not a
pragma.

=cut

sub pragma {
	my $self   = shift;
	my $module = $self->module or return '';
	$module =~ /^[a-z][a-z\d]*$/ ? $module : '';
}

=pod

=head2 version

The C<version> method checks for an include statement that introduces a
dependency on the version of C<perl> the code is compatible with.

This covers two specific statements.

  use 5.006;
  require 5.006;

Currently the version is returned as a string, although in future the version
may be returned as a L<version> object.  If you want a numeric representation,
use C<version_literal()>.  Returns false if the statement is not a version
dependency.

=cut

sub version {
	my $self    = shift;
	my $version = $self->schild(1) or return undef;
	$version->isa('PPI::Token::Number') ? $version->content : '';
}

=pod

=head2 version_literal

The C<version_literal> method has the same behavior as C<version()>, but the
version is returned as a numeric literal.  Returns false if the statement is
not a version dependency.

=cut

sub version_literal {
	my $self    = shift;
	my $version = $self->schild(1) or return undef;
	$version->isa('PPI::Token::Number') ? $version->literal : '';
}

=pod

=head2 arguments

The C<arguments> method gives you the rest of the statement after the
module/pragma and module version, i.e. the stuff that will be used to
construct what gets passed to the module's C<import()> subroutine.  This does
include the comma, etc. operators, but doesn't include non-significant direct
children or any final semicolon.

=cut

sub arguments {
	my $self = shift;
	my @args = $self->schildren;

	# Remove the "use", "no" or "require"
	shift @args;

	# Remove the statement terminator
	if (
		$args[-1]->isa('PPI::Token::Structure')
		and
		$args[-1]->content eq ';'
	) {
		pop @args;
	}

	# Remove the module or perl version.
	shift @args;  

	return unless @args;

	if ( $args[0]->isa('PPI::Token::Number') ) {
		my $after = $args[1] or return;
		$after->isa('PPI::Token::Operator') or shift @args;
	}

	return @args;
}

1;

=pod

=head1 TO DO

- Write specific unit tests for this package

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
