package MooseX::Declare::Context::Parameterized;
# ABSTRACT: context for parsing optionally parameterized statements

our $VERSION = '0.43';

use Moose::Role;
use MooseX::Types::Moose qw/Str HashRef/;

use namespace::autoclean;

#pod =head1 DESCRIPTION
#pod
#pod This context trait will add optional parameterization functionality to the
#pod context.
#pod
#pod =attr parameter_signature
#pod
#pod This will be set when the C<strip_parameter_signature> method is called and it
#pod was able to extract a list of parameterisations.
#pod
#pod =method has_parameter_signature
#pod
#pod Predicate method for the C<parameter_signature> attribute.
#pod
#pod =cut

has parameter_signature => (
    is        => 'rw',
    isa       => Str,
    predicate => 'has_parameter_signature',
);

#pod =method add_parameter
#pod
#pod Allows storing parameters extracted from C<parameter_signature> to be used
#pod later on.
#pod
#pod =method get_parameters
#pod
#pod Returns all previously added parameters.
#pod
#pod =cut

has parameters => (
    traits    => ['Hash'],
    isa       => HashRef,
    default   => sub { {} },
    handles   => {
        add_parameter  => 'set',
        get_parameters => 'kv',
    },
);

#pod =method strip_parameter_signature
#pod
#pod   Maybe[Str] Object->strip_parameter_signature()
#pod
#pod This method is intended to parse the main namespace of a namespaced keyword.
#pod It will use L<Devel::Declare::Context::Simple>s C<strip_word> method and store
#pod the result in the L</namespace> attribute if true.
#pod
#pod =cut

sub strip_parameter_signature {
    my ($self) = @_;

    my $signature = $self->strip_proto;

    $self->parameter_signature($signature)
        if defined $signature && length $signature;

    return $signature;
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

MooseX::Declare::Context::Parameterized - context for parsing optionally parameterized statements

=head1 VERSION

version 0.43

=head1 DESCRIPTION

This context trait will add optional parameterization functionality to the
context.

=head1 ATTRIBUTES

=head2 parameter_signature

This will be set when the C<strip_parameter_signature> method is called and it
was able to extract a list of parameterisations.

=head1 METHODS

=head2 has_parameter_signature

Predicate method for the C<parameter_signature> attribute.

=head2 add_parameter

Allows storing parameters extracted from C<parameter_signature> to be used
later on.

=head2 get_parameters

Returns all previously added parameters.

=head2 strip_parameter_signature

  Maybe[Str] Object->strip_parameter_signature()

This method is intended to parse the main namespace of a namespaced keyword.
It will use L<Devel::Declare::Context::Simple>s C<strip_word> method and store
the result in the L</namespace> attribute if true.

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
