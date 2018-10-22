package MooseX::Role::Parameterized::Meta::Role::Parameterizable;
use Moose;
extends 'Moose::Meta::Role';

our $VERSION = '0.27';

use MooseX::Role::Parameterized::Meta::Role::Parameterized;
use MooseX::Role::Parameterized::Parameters;

has parameterized_role_metaclass => (
    is      => 'ro',
    isa     => 'ClassName',
    default => 'MooseX::Role::Parameterized::Meta::Role::Parameterized',
);

has parameters_class => (
    is      => 'ro',
    isa     => 'ClassName',
    default => 'MooseX::Role::Parameterized::Parameters',
);

has parameters_metaclass => (
    is      => 'rw',
    isa     => 'Moose::Meta::Class',
    lazy    => 1,
    builder => '_build_parameters_metaclass',
    handles => {
        has_parameter        => 'has_attribute',
        add_parameter        => 'add_attribute',
        construct_parameters => 'new_object',
    },
);

has role_generator => (
    is        => 'rw',
    isa       => 'CodeRef',
    predicate => 'has_role_generator',
);

sub _build_parameters_metaclass {
    my $self = shift;

    return $self->parameters_class->meta->create_anon_class(
        superclasses => [$self->parameters_class],
    );
}

sub generate_role {
    my $self     = shift;
    my %args     = @_;

    my $parameters = blessed($args{parameters})
                   ? $args{parameters}
                   : $self->construct_parameters(%{ $args{parameters} });

    confess "A role generator is required to apply parameterized roles (did you forget the 'role { ... }' block in your parameterized role '".$self->name."'?)"
        unless $self->has_role_generator;

    my $parameterized_role_metaclass = $self->parameterized_role_metaclass;
    Class::MOP::load_class($parameterized_role_metaclass);

    my $role;
    if ($args{package}) {
        $role = $parameterized_role_metaclass->create(
            $args{package},
            genitor    => $self,
            parameters => $parameters,
        );
    } else {
        $role = $parameterized_role_metaclass->create_anon_role(
            genitor    => $self,
            parameters => $parameters,
        );
    }

    local $MooseX::Role::Parameterized::CURRENT_METACLASS = $role;

    $self->apply_parameterizable_role($role);

    $self->role_generator->($parameters,
        operating_on => $role,
        consumer     => $args{consumer},
    );

    # don't just return $role here, because it might have been changed when
    # metaroles are applied
    return $MooseX::Role::Parameterized::CURRENT_METACLASS;
}

sub _role_for_combination {
    my $self = shift;
    my $parameters = shift;

    return $self->generate_role(
        parameters => $parameters,
    );
}

sub apply {
    my $self     = shift;
    my $consumer = shift;
    my %args     = @_;

    my $role = $self->generate_role(
        consumer   => $consumer,
        parameters => \%args,
    );

    $role->apply($consumer, %args);
}

sub apply_parameterizable_role {
    my $self = shift;

    $self->SUPER::apply(@_);
}

__PACKAGE__->meta->make_immutable;
no Moose;

1;

__END__

=head1 NAME

MooseX::Role::Parameterized::Meta::Role::Parameterizable - metaclass for parameterizable roles

=head1 DESCRIPTION

This is the metaclass for parameterizable roles, roles that have
their parameters currently unbound. These are the roles that you
use L<Moose/with>, but instead of composing the parameterizable
role, we construct a new parameterized role
(L<MooseX::Role::Parameterized::Meta::Role::Parameterized>) and use
that new parameterized instead.

=head1 ATTRIBUTES

=head2 parameterized_role_metaclass

The name of the class that will be used to construct the parameterized role.

=head2 parameters_class

The name of the class that will be used to construct the parameters object.

=head2 parameters_metaclass

A metaclass representing this roles's parameters. It will be an anonymous
subclass of L</parameters_class>. Each call to
L<MooseX::Role::Parameters/parameter> adds an attribute to this metaclass.

When this role is consumed, the parameters object will be instantiated using
this metaclass.

=head2 role_generator

A code reference that is used to generate a role based on the parameters
provided by the consumer. The user usually specifies it using the
L<MooseX::Role::Parameterized/role> keyword.

=head1 METHODS

=head2 add_parameter $name, %options

Delegates to L<Moose::Meta::Class/add_attribute> on the
L</parameters_metaclass> object.

=head2 construct_parameters %arguments

Creates a new L<MooseX::Role::Parameterized::Parameters> object using metaclass
L</parameters_metaclass>.

The arguments are those specified by the consumer as parameter values.

=head2 generate_role %arguments

This method generates and returns a new instance of
L</parameterized_role_metaclass>. It can take any combination of
three named parameters:

=over 4

=item arguments

A hashref of parameters for the role, same as would be passed in at a "with"
statement.

=item package

A package name that, if present, we will use for the generated role; if not,
we generate an anonymous role.

=item consumer

A consumer metaobject, if available.

=back

=head2 apply

Overrides L<Moose::Meta::Role/apply> to automatically generate the
parameterized role.

=cut

