package Parse::Method::Signatures::Param;

use Moose;
use MooseX::Types::Structured qw/Tuple/;
use MooseX::Types::Moose qw/Bool Str ArrayRef HashRef/;

use namespace::clean -except => 'meta';

with 'MooseX::Traits';

has required => (
    is       => 'ro',
    isa      => Bool,
    required => 1
);

has sigil => (
    is       => 'ro',
    isa      => Str,
    required => 1,
);

has type_constraints => (
    is         => 'ro',
    isa        => 'Parse::Method::Signatures::TypeConstraint',
    predicate  => 'has_type_constraints',
    handles    => {
        meta_type_constraint => 'tc'
    },
);

has default_value => (
    is        => 'ro',
    isa       => Str,
    predicate => 'has_default_value',
);

has constraints => (
    is         => 'ro',
    isa        => ArrayRef[Str],
    predicate  => 'has_constraints',
    auto_deref => 1,
);

has param_traits => (
    is         => 'ro',
    isa        => ArrayRef[Tuple[Str, Str]],
    predicate  => 'has_traits',
    auto_deref => 1
);

has '+_trait_namespace' => (
    default => 'Parse::Method::Signatures::Param',
);

sub _stringify_type_constraints {
    my ($self) = @_;
    return $self->has_type_constraints
        ? $self->type_constraints->to_string . q{ }
        : q{};
}

sub _stringify_default_value {
    my ($self) = @_;
    return $self->has_default_value
        ? q{ = } . $self->default_value
        : q{};
}

sub _stringify_constraints {
    my ($self) = @_;
    return q{} unless $self->has_constraints;
    return q{ where } . join(q{ where }, $self->constraints);
}

sub _stringify_traits {
    my ($self) = @_;
    return q{} unless $self->has_traits;
    return q{ } . join q{ }, map { @{ $_ } } $self->param_traits;
}

sub to_string {
    my ($self) = @_;
    my $ret = q{};

    $ret .= $self->_stringify_type_constraints;
    $ret .= $self->_stringify_variable_name;
    $ret .= $self->_stringify_required;
    $ret .= $self->_stringify_default_value;
    $ret .= $self->_stringify_constraints;
    $ret .= $self->_stringify_traits;

    return $ret;
}

__PACKAGE__->meta->make_immutable;

1;

=head1 NAME

Parse::Method::Signatures::Param - a parsed parameter from a signature

=head1 ATTRIBUTES

All attributes of this class are read-only.

=head2 required

Is this parameter required (true) or optional (false)?

=head2 sigil

The effective sigil ('$', '@' or '%') of this parameter.

=head2 type_constraints

=over

B<Type:> L<Parse::Method::Signatures::TypeConstraint>

B<Predicate:> has_type_constraints

=back

Representation of the type constraint for this parameter. Most commonly you
will just call L</meta_type_constraint> and not access this attribute directly.

=head2 default_value

=over

B<Type:> Str

B<Predicate:> has_default_value

=back

A string that should be eval'd or injected to get the default value for this
parameter. For example:

 $name = 'bar'

Would give a default_value of "'bar'".

=head2 constraints

=over

B<Type:> ArrayRef[Str]

B<Predicate:> has_constraints

=back

C<where> constraints for this type. Each member of the array a the string
(including enclosing braces) of the where constraint block.

=head2 param_traits

=over

B<Type:> ArrayRef[ Tupple[Str,Str] ]

B<Predicate:> has_traits

=back

Traits that this parameter is declared to have. For instance

 $foo does coerce

would have a trait of

 ['does', 'coerce']

=head1 METHODS

=head2 to_string

=head2 meta_type_constraint

Get the L<Moose::Meta::TypeConstraint> for this parameter. Check first that the
type has a type constraint:

 $tc = $param->meta_type_constraint if $param->has_type_constraints;

=head1 SEE ALSO

L<Parse::Method::Signatures>.

=head1 AUTHORS

Ash Berlin <ash@cpan.org>.

Florian Ragwitz <rafl@debian.org>.

=head1 LICENSE

Licensed under the same terms as Perl itself.

