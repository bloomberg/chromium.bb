package MooseX::Declare::Context::Namespaced;
# ABSTRACT: Namespaced context

our $VERSION = '0.43';

use Moose::Role;
use Carp                  qw( croak );
use MooseX::Declare::Util qw( outer_stack_peek );
use namespace::autoclean;

#pod =head1 DESCRIPTION
#pod
#pod This context trait will add namespace functionality to the context.
#pod
#pod =attr namespace
#pod
#pod This will be set when the C<strip_namespace> method is called and the
#pod namespace wasn't anonymous. It will contain the specified namespace, not
#pod the fully qualified one.
#pod
#pod =cut

has namespace => (
    is          => 'rw',
    isa         => 'Str',
);


#pod =method strip_namespace
#pod
#pod   Maybe[Str] Object->strip_namespace()
#pod
#pod This method is intended to parse the main namespace of a namespaced keyword.
#pod It will use L<Devel::Declare::Context::Simple>s C<strip_word> method and store
#pod the result in the L</namespace> attribute if true.
#pod
#pod =cut

sub strip_namespace {
    my ($self) = @_;

    my $namespace = $self->strip_word;

    $self->namespace($namespace)
        if defined $namespace and length $namespace;

    return $namespace;
}

#pod =method qualify_namespace
#pod
#pod   Str Object->qualify_namespace(Str $namespace)
#pod
#pod If the C<$namespace> passed it begins with a C<::>, it will be prefixed with
#pod the outer namespace in the file. If there is no outer namespace, an error
#pod will be thrown.
#pod
#pod =cut

sub qualify_namespace {
    my ($self, $namespace) = @_;

    # only qualify namespaces starting with ::
    return $namespace
        unless $namespace =~ /^::/;

    # try to find the enclosing package
    my $outer = outer_stack_peek($self->caller_file)
        or croak "No outer namespace found to apply relative $namespace to";

    return $outer . $namespace;
}

#pod =head1 SEE ALSO
#pod
#pod =for :list
#pod * L<MooseX::Declare>
#pod * L<MooseX::Declare::Context>
#pod
#pod =cut

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

MooseX::Declare::Context::Namespaced - Namespaced context

=head1 VERSION

version 0.43

=head1 DESCRIPTION

This context trait will add namespace functionality to the context.

=head1 ATTRIBUTES

=head2 namespace

This will be set when the C<strip_namespace> method is called and the
namespace wasn't anonymous. It will contain the specified namespace, not
the fully qualified one.

=head1 METHODS

=head2 strip_namespace

  Maybe[Str] Object->strip_namespace()

This method is intended to parse the main namespace of a namespaced keyword.
It will use L<Devel::Declare::Context::Simple>s C<strip_word> method and store
the result in the L</namespace> attribute if true.

=head2 qualify_namespace

  Str Object->qualify_namespace(Str $namespace)

If the C<$namespace> passed it begins with a C<::>, it will be prefixed with
the outer namespace in the file. If there is no outer namespace, an error
will be thrown.

=head1 SEE ALSO

=over 4

=item *

L<MooseX::Declare>

=item *

L<MooseX::Declare::Context>

=back

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2008 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
