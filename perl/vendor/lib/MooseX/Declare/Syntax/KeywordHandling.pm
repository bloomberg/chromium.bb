package MooseX::Declare::Syntax::KeywordHandling;
# ABSTRACT: Basic keyword functionality

our $VERSION = '0.43';

use Moose::Role;
use Moose::Util::TypeConstraints qw(subtype as where);
use Devel::Declare ();
use Sub::Install qw( install_sub );
use Moose::Meta::Class ();
use Module::Runtime 'use_module';

use aliased 'MooseX::Declare::Context';

use namespace::autoclean -also => ['_uniq'];

#pod =head1 DESCRIPTION
#pod
#pod This role provides the functionality common for all keyword handlers
#pod in L<MooseX::Declare>.
#pod
#pod =head1 REQUIRED METHODS
#pod
#pod =head2 parse
#pod
#pod   Object->parse (Object $context)
#pod
#pod This method must implement the actual parsing of the keyword syntax.
#pod
#pod =cut

requires qw(
    parse
);

#pod =attr identifier
#pod
#pod This is the name of the actual keyword. It is a required string that is in
#pod the same format as a usual Perl identifier.
#pod
#pod =cut

has identifier => (
    is          => 'ro',
    isa         => subtype(as 'Str', where { /^ [_a-z] [_a-z0-9]* $/ix }),
    required    => 1,
);

#pod =method get_identifier
#pod
#pod   Str Object->get_identifier ()
#pod
#pod Returns the name the handler will be setup under.
#pod
#pod =cut

sub get_identifier { shift->identifier }

sub context_class { Context }

sub context_traits { }

#pod =method setup_for
#pod
#pod   Object->setup_for (ClassName $class, %args)
#pod
#pod This will setup the handler in the specified C<$class>. The handler will
#pod dispatch to the L</parse_declaration> method when the keyword is used.
#pod
#pod A normal code reference will also be exported into the calling namespace.
#pod It will either be empty or, if a C<generate_export> method is provided,
#pod the return value of that method.
#pod
#pod =cut

sub setup_for {
    my ($self, $setup_class, %args) = @_;

    # make sure the stack is valid
    my $stack = $args{stack} || [];
    my $ident = $self->get_identifier;

    # setup the D:D handler for our keyword
    Devel::Declare->setup_for($setup_class, {
        $ident => {
            const => sub { $self->parse_declaration((caller(1))[1], \%args, @_) },
        },
    });

    # search or generate a real export
    my $export = $self->can('generate_export') ? $self->generate_export($setup_class) : sub { };

    # export subroutine
    install_sub({
        code    => $export,
        into    => $setup_class,
        as      => $ident,
    }) unless $setup_class->can($ident);

    return 1;
}

#pod =method parse_declaration
#pod
#pod   Object->parse_declaration (Str $filename, HashRef $setup_args, @call_args)
#pod
#pod This simply creates a new L<context|MooseX::Declare::Context> and passes it
#pod to the L</parse> method.
#pod
#pod =cut

sub parse_declaration {
    my ($self, $caller_file, $args, @ctx_args) = @_;

    # find and load context object class
    my $ctx_class = $self->context_class;
    use_module $ctx_class;

    # do we have traits?
    if (my @ctx_traits = _uniq($self->context_traits)) {

        use_module $_
            for @ctx_traits;

        $ctx_class = Moose::Meta::Class->create_anon_class(
            superclasses => [$ctx_class],
            roles        => [@ctx_traits],
            cache        => 1,
        )->name;
    }

    # create a context object and initialize it
    my $ctx = $ctx_class->new(
        %{ $args },
        caller_file => $caller_file,
    );
    $ctx->init(@ctx_args);

    # parse with current context
    return $self->parse($ctx);
}

sub _uniq { keys %{ +{ map { $_ => undef } @_ } } }

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

MooseX::Declare::Syntax::KeywordHandling - Basic keyword functionality

=head1 VERSION

version 0.43

=head1 DESCRIPTION

This role provides the functionality common for all keyword handlers
in L<MooseX::Declare>.

=head1 ATTRIBUTES

=head2 identifier

This is the name of the actual keyword. It is a required string that is in
the same format as a usual Perl identifier.

=head1 METHODS

=head2 get_identifier

  Str Object->get_identifier ()

Returns the name the handler will be setup under.

=head2 setup_for

  Object->setup_for (ClassName $class, %args)

This will setup the handler in the specified C<$class>. The handler will
dispatch to the L</parse_declaration> method when the keyword is used.

A normal code reference will also be exported into the calling namespace.
It will either be empty or, if a C<generate_export> method is provided,
the return value of that method.

=head2 parse_declaration

  Object->parse_declaration (Str $filename, HashRef $setup_args, @call_args)

This simply creates a new L<context|MooseX::Declare::Context> and passes it
to the L</parse> method.

=head1 REQUIRED METHODS

=head2 parse

  Object->parse (Object $context)

This method must implement the actual parsing of the keyword syntax.

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
