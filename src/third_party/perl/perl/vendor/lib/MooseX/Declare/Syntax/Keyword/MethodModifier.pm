package MooseX::Declare::Syntax::Keyword::MethodModifier;
# ABSTRACT: Handle method modifier declarations

our $VERSION = '0.43';

use Moose;
use Moose::Util;
use Moose::Util::TypeConstraints 'enum';
use namespace::autoclean;

#pod =head1 DESCRIPTION
#pod
#pod Allows the implementation of method modification handlers like C<around> and
#pod C<before>.
#pod
#pod =head1 CONSUMES
#pod
#pod =for :list
#pod * L<MooseX::Declare::Syntax::MethodDeclaration>
#pod
#pod =cut

with 'MooseX::Declare::Syntax::MethodDeclaration';

#pod =attr modifier_type
#pod
#pod A required string that is one of:
#pod
#pod =for :list
#pod * around
#pod * after
#pod * before
#pod * override
#pod * augment
#pod
#pod =cut

has modifier_type => (
    is          => 'rw',
    isa         => enum([qw( around after before override augment )]),
    required    => 1,
);

#pod =method register_method_declaration
#pod
#pod   Object->register_method_declaration (Object $metaclass, Str $name, Object $method)
#pod
#pod This will add the method modifier to the C<$metaclass> via L<Moose::Util>s
#pod C<add_method_modifier>, whose return value will also be returned from this
#pod method.
#pod
#pod =cut

sub register_method_declaration {
    my ($self, $meta, $name, $method) = @_;
    return Moose::Util::add_method_modifier($meta->name, $self->modifier_type, [$name, $method->body]);
}

#pod =head1 SEE ALSO
#pod
#pod =for :list
#pod * L<MooseX::Declare>
#pod * L<MooseX::Declare::Syntax::MooseSetup>
#pod * L<MooseX::Declare::Syntax::MethodDeclaration>
#pod * L<MooseX::Method::Signatures>
#pod
#pod =cut

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

MooseX::Declare::Syntax::Keyword::MethodModifier - Handle method modifier declarations

=head1 VERSION

version 0.43

=head1 DESCRIPTION

Allows the implementation of method modification handlers like C<around> and
C<before>.

=head1 ATTRIBUTES

=head2 modifier_type

A required string that is one of:

=over 4

=item *

around

=item *

after

=item *

before

=item *

override

=item *

augment

=back

=head1 METHODS

=head2 register_method_declaration

  Object->register_method_declaration (Object $metaclass, Str $name, Object $method)

This will add the method modifier to the C<$metaclass> via L<Moose::Util>s
C<add_method_modifier>, whose return value will also be returned from this
method.

=head1 CONSUMES

=over 4

=item *

L<MooseX::Declare::Syntax::MethodDeclaration>

=back

=head1 SEE ALSO

=over 4

=item *

L<MooseX::Declare>

=item *

L<MooseX::Declare::Syntax::MooseSetup>

=item *

L<MooseX::Declare::Syntax::MethodDeclaration>

=item *

L<MooseX::Method::Signatures>

=back

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2008 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
