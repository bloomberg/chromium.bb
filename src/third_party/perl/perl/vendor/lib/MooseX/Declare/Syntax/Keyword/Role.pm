package MooseX::Declare::Syntax::Keyword::Role;
# ABSTRACT: Role declarations

our $VERSION = '0.43';

use Moose;
use Moose::Util qw(does_role find_meta);
use aliased 'Parse::Method::Signatures' => 'PMS';
use aliased 'MooseX::Declare::Syntax::MethodDeclaration';
use aliased 'Parse::Method::Signatures::Param::Placeholder';
use aliased 'MooseX::Declare::Context::Parameterized', 'ParameterizedCtx';
use aliased 'MooseX::Declare::Syntax::MethodDeclaration::Parameterized', 'ParameterizedMethod';

use namespace::autoclean;

#pod =head1 CONSUMES
#pod
#pod =for :list
#pod * L<MooseX::Declare::Syntax::MooseSetup>
#pod * L<MooseX::Declare::Syntax::RoleApplication>
#pod
#pod =cut

with qw(
    MooseX::Declare::Syntax::MooseSetup
    MooseX::Declare::Syntax::RoleApplication
);

#pod =head1 MODIFIED METHODS
#pod
#pod =head2 imported_moose_symbols
#pod
#pod   List Object->imported_moose_symbols ()
#pod
#pod Extends the existing L<MooseX::Declare::Syntax::MooseSetup/imported_moose_symbols>
#pod with C<requires>, C<extends>, C<has>, C<inner> and C<super>.
#pod
#pod =cut

around imported_moose_symbols => sub { shift->(@_), qw( requires excludes extends has inner super ) };

#pod =head2 import_symbols_from
#pod
#pod   Str Object->import_symbols_from ()
#pod
#pod Will return L<Moose::Role> instead of the default L<Moose>.
#pod
#pod =cut

around import_symbols_from => sub {
    my ($next, $self, $ctx) = @_;
    return $ctx->has_parameter_signature
        ? 'MooseX::Role::Parameterized'
        : 'Moose::Role';
};

#pod =head2 make_anon_metaclass
#pod
#pod   Object Object->make_anon_metaclass ()
#pod
#pod This will return an anonymous instance of L<Moose::Meta::Role>.
#pod
#pod =cut

around make_anon_metaclass => sub { Moose::Meta::Role->create_anon_role };

around context_traits => sub { shift->(@_), ParameterizedCtx };

around default_inner => sub {
    my ($next, $self, $stack) = @_;
    my $inner = $self->$next;
    return $inner
        if !@{ $stack || [] } || !$stack->[-1]->is_parameterized;

    ParameterizedMethod->meta->apply($_)
        for grep { does_role($_, MethodDeclaration) } @{ $inner };

    return $inner;
};

#pod =method generate_export
#pod
#pod   CodeRef Object->generate_export ()
#pod
#pod Returns a closure with a call to L</make_anon_metaclass>.
#pod
#pod =cut

sub generate_export { my $self = shift; sub { $self->make_anon_metaclass } }

after parse_namespace_specification => sub {
    my ($self, $ctx) = @_;
    $ctx->strip_parameter_signature;
};

after add_namespace_customizations => sub {
    my ($self, $ctx, $package, $options) = @_;
    $self->add_parameterized_customizations($ctx, $package, $options)
        if $ctx->has_parameter_signature;
};

sub add_parameterized_customizations {
    my ($self, $ctx, $package, $options) = @_;

    my $sig = PMS->signature(
        input          => "(${\$ctx->parameter_signature})",
        from_namespace => $ctx->get_curstash_name,
    );
    confess 'Positional parameters are not allowed in parameterized roles'
        if $sig->has_positional_params;

    my @vars = map {
        does_role($_, Placeholder)
            ? ()
            : {
                var  => $_->variable_name,
                name => $_->label,
                tc   => $_->meta_type_constraint,
                ($_->has_default_value
                    ? (default => $_->default_value)
                    : ()),
            },
    } $sig->named_params;

    $ctx->add_preamble_code_parts(
        sprintf 'my (%s) = map { $_[0]->$_ } qw(%s);',
            join(',', map { $_->{var}  } @vars),
            join(' ', map { $_->{name} } @vars),
    );

    for my $var (@vars) {
        $ctx->add_parameter($var->{name} => {
            is  => 'ro',
            isa => $var->{tc},
            (exists $var->{default}
                ? (default => sub { eval $var->{default} })
                : ()),
        });
    }
}

after handle_post_parsing => sub {
    my ($self, $ctx, $package, $class) = @_;
    return unless $ctx->has_parameter_signature;
    $ctx->shadow(sub (&) {
        my $meta = find_meta($class);
        $meta->add_parameter($_->[0], %{ $_->[1] })
            for $ctx->get_parameters;
        $meta->role_generator($_[0]);
        return $class;
    });
};

#pod =head1 SEE ALSO
#pod
#pod =for :list
#pod * L<MooseX::Declare>
#pod * L<MooseX::Declare::Syntax::Keyword::Class>
#pod * L<MooseX::Declare::Syntax::RoleApplication>
#pod * L<MooseX::Declare::Syntax::MooseSetup>
#pod
#pod =cut

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

MooseX::Declare::Syntax::Keyword::Role - Role declarations

=head1 VERSION

version 0.43

=head1 METHODS

=head2 generate_export

  CodeRef Object->generate_export ()

Returns a closure with a call to L</make_anon_metaclass>.

=head1 CONSUMES

=over 4

=item *

L<MooseX::Declare::Syntax::MooseSetup>

=item *

L<MooseX::Declare::Syntax::RoleApplication>

=back

=head1 MODIFIED METHODS

=head2 imported_moose_symbols

  List Object->imported_moose_symbols ()

Extends the existing L<MooseX::Declare::Syntax::MooseSetup/imported_moose_symbols>
with C<requires>, C<extends>, C<has>, C<inner> and C<super>.

=head2 import_symbols_from

  Str Object->import_symbols_from ()

Will return L<Moose::Role> instead of the default L<Moose>.

=head2 make_anon_metaclass

  Object Object->make_anon_metaclass ()

This will return an anonymous instance of L<Moose::Meta::Role>.

=head1 SEE ALSO

=over 4

=item *

L<MooseX::Declare>

=item *

L<MooseX::Declare::Syntax::Keyword::Class>

=item *

L<MooseX::Declare::Syntax::RoleApplication>

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
