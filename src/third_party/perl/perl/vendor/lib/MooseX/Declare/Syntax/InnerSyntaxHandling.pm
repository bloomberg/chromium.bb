package MooseX::Declare::Syntax::InnerSyntaxHandling;
# ABSTRACT: Keywords inside blocks

our $VERSION = '0.43';

use Moose::Role;
use MooseX::Declare::Util qw( outer_stack_push );
use namespace::autoclean;

#pod =head1 DESCRIPTION
#pod
#pod This role allows you to setup keyword handlers that are only available
#pod inside blocks or other scoping environments.
#pod
#pod =head1 REQUIRED METHODS
#pod
#pod =head2 get_identifier
#pod
#pod   Str get_identifier ()
#pod
#pod Required to return the name of the identifier of the current handler.
#pod
#pod =cut

requires qw(
    get_identifier
);

#pod =method default_inner
#pod
#pod   ArrayRef[Object] Object->default_inner ()
#pod
#pod Returns an empty C<ArrayRef> by default. If you want to setup additional
#pod keywords you will have to C<around> this method.
#pod
#pod =cut

sub default_inner { [] }

#pod =head1 MODIFIED METHODS
#pod
#pod =head2 setup_for
#pod
#pod   Object->setup_for(ClassName $class, %args)
#pod
#pod After the keyword is setup inside itself, this will call L</setup_inner_for>.
#pod
#pod =cut

after setup_for => sub {
    my ($self, $setup_class, %args) = @_;

    # make sure stack is valid
    my $stack = $args{stack} || [];

    # setup inner keywords if we're inside ourself
    if (grep { $_ eq $self->get_identifier } @$stack) {
        $self->setup_inner_for($setup_class, %args);
    }
};

#pod =method setup_inner_for
#pod
#pod   Object->setup_inner_for(ClassName $class, %args)
#pod
#pod Sets up all handlers in the inner class.
#pod
#pod =cut

sub setup_inner_for {
    my ($self, $setup_class, %args) = @_;

    # setup each keyword in target class
    for my $inner (@{ $self->default_inner($args{stack}) }) {
        $inner->setup_for($setup_class, %args);
    }

    # push package onto stack for namespace management
    if (exists $args{file}) {
        outer_stack_push $args{file}, $args{outer_package};
    }
}

#pod =head1 SEE ALSO
#pod
#pod =for :list
#pod * L<MooseX::Declare>
#pod * L<MooseX::Declare::Syntax::NamespaceHandling>
#pod * L<MooseX::Declare::Syntax::MooseSetup>
#pod
#pod =cut

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

MooseX::Declare::Syntax::InnerSyntaxHandling - Keywords inside blocks

=head1 VERSION

version 0.43

=head1 DESCRIPTION

This role allows you to setup keyword handlers that are only available
inside blocks or other scoping environments.

=head1 METHODS

=head2 default_inner

  ArrayRef[Object] Object->default_inner ()

Returns an empty C<ArrayRef> by default. If you want to setup additional
keywords you will have to C<around> this method.

=head2 setup_inner_for

  Object->setup_inner_for(ClassName $class, %args)

Sets up all handlers in the inner class.

=head1 REQUIRED METHODS

=head2 get_identifier

  Str get_identifier ()

Required to return the name of the identifier of the current handler.

=head1 MODIFIED METHODS

=head2 setup_for

  Object->setup_for(ClassName $class, %args)

After the keyword is setup inside itself, this will call L</setup_inner_for>.

=head1 SEE ALSO

=over 4

=item *

L<MooseX::Declare>

=item *

L<MooseX::Declare::Syntax::NamespaceHandling>

=item *

L<MooseX::Declare::Syntax::MooseSetup>

=back

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2008 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
