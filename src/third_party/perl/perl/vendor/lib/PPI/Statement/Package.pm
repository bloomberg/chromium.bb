package PPI::Statement::Package;

=pod

=head1 NAME

PPI::Statement::Package - A package statement

=head1 INHERITANCE

  PPI::Statement::Package
  isa PPI::Statement
      isa PPI::Node
          isa PPI::Element

=head1 DESCRIPTION

Most L<PPI::Statement> subclasses are assigned based on the value of the
first token or word found in the statement. When PPI encounters a statement
starting with 'package', it converts it to a C<PPI::Statement::Package>
object.

When working with package statements, please remember that packages only
exist within their scope, and proper support for scoping has yet to be
completed in PPI.

However, if the immediate parent of the package statement is the
top level L<PPI::Document> object, then it can be considered to define
everything found until the next top-level "file scoped" package statement.

A file may, however, contain nested temporary package, in which case you
are mostly on your own :)


=begin testing hash_constructors_dont_contain_packages_rt52259 2

my $Document = PPI::Document->new(\<<'END_PERL');
{    package  => "", };
+{   package  => "", };
{   'package' => "", };
+{  'package' => "", };
{   'package' ,  "", };
+{  'package' ,  "", };
END_PERL

isa_ok( $Document, 'PPI::Document' );

my $packages = $Document->find('PPI::Statement::Package');
my $test_name = 'Found no package statements in hash constructors - RT #52259';
if (not $packages) {
	pass $test_name;
} elsif ( not is(scalar @{$packages}, 0, $test_name) ) {
	diag 'Package statements found:';
	diag $_->parent()->parent()->content() foreach @{$packages};
}

=end testing


=head1 METHODS

C<PPI::Statement::Package> has a number of methods in addition to the standard
L<PPI::Statement>, L<PPI::Node> and L<PPI::Element> methods.

=cut

use strict;
use PPI::Statement ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Statement';
}

=pod

=head2 namespace

Most package declarations are simple, and just look something like

  package Foo::Bar;

The C<namespace> method returns the name of the declared package, in the
above case 'Foo::Bar'. It returns this exactly as written and does not
attempt to clean up or resolve things like ::Foo to main::Foo.

If the package statement is done any different way, it returns false.

=cut

sub namespace {
	my $self = shift;
	my $namespace = $self->schild(1) or return '';
	$namespace->isa('PPI::Token::Word')
		? $namespace->content
		: '';
}

=pod

=head2 file_scoped

Regardless of whether it is named or not, the C<file_scoped> method will
test to see if the package declaration is a top level "file scoped"
statement or not, based on its location.

In general, returns true if it is a "file scoped" package declaration with
an immediate parent of the top level Document, or false if not.

Note that if the PPI DOM tree B<does not> have a PPI::Document object at
as the root element, this will return false. Likewise, it will also return
false if the root element is a L<PPI::Document::Fragment>, as a fragment of
a file does not represent a scope.

=cut

sub file_scoped {
	my $self     = shift;
	my ($Parent, $Document) = ($self->parent, $self->top);
	$Parent and $Document and $Parent == $Document
	and $Document->isa('PPI::Document')
	and ! $Document->isa('PPI::Document::Fragment');
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
