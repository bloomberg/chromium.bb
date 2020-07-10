package MooseX::Declare::Syntax::MooseSetup;
# ABSTRACT: Common Moose namespaces declarations

our $VERSION = '0.43';

use Moose::Role;

use Moose::Util  qw( find_meta );
use Sub::Install qw( install_sub );

use aliased 'MooseX::Declare::Syntax::Keyword::MethodModifier';
use aliased 'MooseX::Declare::Syntax::Keyword::Method';
use aliased 'MooseX::Declare::Syntax::Keyword::With', 'WithKeyword';
use aliased 'MooseX::Declare::Syntax::Keyword::Clean', 'CleanKeyword';

use namespace::autoclean;

#pod =head1 DESCRIPTION
#pod
#pod This role is basically an extension to
#pod L<NamespaceHandling|MooseX::Declare::Syntax::NamespaceHandling>. It adds all
#pod the common parts for L<Moose> namespace definitions. Examples of this role
#pod can be found in the L<class|MooseX::Declare::Syntax::Keyword::Class> and
#pod L<role|MooseX::Declare::Syntax::Keyword::Role> keywords.
#pod
#pod =head1 CONSUMES
#pod
#pod =for :list
#pod * L<MooseX::Declare::Syntax::NamespaceHandling>
#pod * L<MooseX::Declare::Syntax::EmptyBlockIfMissing>
#pod
#pod =cut

with qw(
    MooseX::Declare::Syntax::NamespaceHandling
    MooseX::Declare::Syntax::EmptyBlockIfMissing
);

#pod =method auto_make_immutable
#pod
#pod   Bool Object->auto_make_immutable ()
#pod
#pod Since L<Moose::Role>s can't be made immutable (this is not a bug or a
#pod missing feature, it would make no sense), this always returns false.
#pod
#pod =cut

sub auto_make_immutable { 0 }

#pod =method imported_moose_symbols
#pod
#pod   List Object->imported_moose_symbols ()
#pod
#pod This will return C<confess> and C<blessed> by default to provide as
#pod additional imports to the namespace.
#pod
#pod =cut

sub imported_moose_symbols { qw( confess blessed ) }

#pod =method import_symbols_from
#pod
#pod   Str Object->import_symbols_from ()
#pod
#pod The namespace from which the additional imports will be imported. This
#pod will return C<Moose> by default.
#pod
#pod =cut

sub import_symbols_from { 'Moose' }

#pod =head1 MODIFIED METHODS
#pod
#pod =head2 default_inner
#pod
#pod   ArrayRef default_inner ()
#pod
#pod This will provide the following default inner-handlers to the namespace:
#pod
#pod =for :list
#pod * method
#pod A simple L<Method|MooseX::Declare::Syntax::Keyword::Method> handler.
#pod * around
#pod This is a L<MethodModifier|MooseX::Declare::Syntax::Keyword::MethodModifier>
#pod handler that will start the signature of the generated method with
#pod C<$orig: $self> to provide the original method in C<$orig>.
#pod * after
#pod * before
#pod * override
#pod * augment
#pod These four handlers are L<MethodModifier|MooseX::Declare::Syntax::Keyword::MethodModifier>
#pod instances.
#pod * clean
#pod This is an instance of the L<Clean|MooseX::Declare::Syntax::Keyword::Clean> keyword
#pod handler.
#pod
#pod The original method will never be called and all arguments are ignored at the
#pod moment.
#pod
#pod =cut

around default_inner => sub {
    return [
        WithKeyword->new(identifier => 'with'),
        Method->new(identifier => 'method'),
        MethodModifier->new(
            identifier           => 'around',
            modifier_type        => 'around',
            prototype_injections => {
                declarator => 'around',
                injections => [ 'CodeRef $orig' ],
            },
        ),
        map { MethodModifier->new(identifier => $_, modifier_type => $_) }
            qw( after before override augment ),
    ];
};

#pod =head2 setup_inner_for
#pod
#pod   Object->setup_inner_for (ClassName $class)
#pod
#pod This will install a C<with> function that will push its arguments onto a global
#pod storage array holding the roles of the current namespace.
#pod
#pod =cut

after setup_inner_for => sub {
    my ($self, $setup_class, %args) = @_;
    my $keyword = CleanKeyword->new(identifier => 'clean');
    $keyword->setup_for($setup_class, %args);
};

#pod =head2 add_namespace_customizations
#pod
#pod   Object->add_namespace_customizations (Object $context, Str $package, HashRef $options)
#pod
#pod After all other customizations, this will first add code to import the
#pod L</imported_moose_symbols> from the package returned in L</import_symbols_from> to
#pod the L<preamble|MooseX::Declare::Context/preamble_code_parts>.
#pod
#pod Then it will add a code part that will immutabilize the class to the
#pod L<cleanup|MooseX::Declare::Context/cleanup_code_parts> code if the
#pod L</auto_make_immutable> method returned a true value and C<< $options->{is}{mutable} >>
#pod does not exist.
#pod
#pod =cut

after add_namespace_customizations => sub {
    my ($self, $ctx, $package) = @_;

    # add Moose initializations to preamble
    $ctx->add_preamble_code_parts(
        sprintf 'use %s qw( %s )', $self->import_symbols_from($ctx), join ' ', $self->imported_moose_symbols($ctx),
    );

    # make class immutable unless specified otherwise
    $ctx->add_cleanup_code_parts(
        "${package}->meta->make_immutable",
    ) if $self->auto_make_immutable
         and not exists $ctx->options->{is}{mutable};
};

#pod =head2 handle_post_parsing
#pod
#pod   CodeRef Object->handle_post_parsing (Object $context, Str $package, Str|Object $name)
#pod
#pod Generates a callback that sets up the roles in the global role storage for the current
#pod namespace. The C<$name> parameter will be the specified name (in contrast to C<$package>
#pod which will always be the fully qualified name) or the anonymous metaclass instance if
#pod none was specified.
#pod
#pod =cut

after handle_post_parsing => sub {
    my ($self, $ctx, $package, $class) = @_;
    $ctx->shadow(sub (&) { shift->(); return $class; });
};


#pod =head1 SEE ALSO
#pod
#pod =for :list
#pod * L<MooseX::Declare>
#pod * L<Moose>
#pod
#pod =cut

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

MooseX::Declare::Syntax::MooseSetup - Common Moose namespaces declarations

=head1 VERSION

version 0.43

=head1 DESCRIPTION

This role is basically an extension to
L<NamespaceHandling|MooseX::Declare::Syntax::NamespaceHandling>. It adds all
the common parts for L<Moose> namespace definitions. Examples of this role
can be found in the L<class|MooseX::Declare::Syntax::Keyword::Class> and
L<role|MooseX::Declare::Syntax::Keyword::Role> keywords.

=head1 METHODS

=head2 auto_make_immutable

  Bool Object->auto_make_immutable ()

Since L<Moose::Role>s can't be made immutable (this is not a bug or a
missing feature, it would make no sense), this always returns false.

=head2 imported_moose_symbols

  List Object->imported_moose_symbols ()

This will return C<confess> and C<blessed> by default to provide as
additional imports to the namespace.

=head2 import_symbols_from

  Str Object->import_symbols_from ()

The namespace from which the additional imports will be imported. This
will return C<Moose> by default.

=head1 CONSUMES

=over 4

=item *

L<MooseX::Declare::Syntax::NamespaceHandling>

=item *

L<MooseX::Declare::Syntax::EmptyBlockIfMissing>

=back

=head1 MODIFIED METHODS

=head2 default_inner

  ArrayRef default_inner ()

This will provide the following default inner-handlers to the namespace:

=over 4

=item *

method

A simple L<Method|MooseX::Declare::Syntax::Keyword::Method> handler.

=item *

around

This is a L<MethodModifier|MooseX::Declare::Syntax::Keyword::MethodModifier>
handler that will start the signature of the generated method with
C<$orig: $self> to provide the original method in C<$orig>.

=item *

after

=item *

before

=item *

override

=item *

augment

These four handlers are L<MethodModifier|MooseX::Declare::Syntax::Keyword::MethodModifier>
instances.

=item *

clean

This is an instance of the L<Clean|MooseX::Declare::Syntax::Keyword::Clean> keyword
handler.

=back

The original method will never be called and all arguments are ignored at the
moment.

=head2 setup_inner_for

  Object->setup_inner_for (ClassName $class)

This will install a C<with> function that will push its arguments onto a global
storage array holding the roles of the current namespace.

=head2 add_namespace_customizations

  Object->add_namespace_customizations (Object $context, Str $package, HashRef $options)

After all other customizations, this will first add code to import the
L</imported_moose_symbols> from the package returned in L</import_symbols_from> to
the L<preamble|MooseX::Declare::Context/preamble_code_parts>.

Then it will add a code part that will immutabilize the class to the
L<cleanup|MooseX::Declare::Context/cleanup_code_parts> code if the
L</auto_make_immutable> method returned a true value and C<< $options->{is}{mutable} >>
does not exist.

=head2 handle_post_parsing

  CodeRef Object->handle_post_parsing (Object $context, Str $package, Str|Object $name)

Generates a callback that sets up the roles in the global role storage for the current
namespace. The C<$name> parameter will be the specified name (in contrast to C<$package>
which will always be the fully qualified name) or the anonymous metaclass instance if
none was specified.

=head1 SEE ALSO

=over 4

=item *

L<MooseX::Declare>

=item *

L<Moose>

=back

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2008 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
