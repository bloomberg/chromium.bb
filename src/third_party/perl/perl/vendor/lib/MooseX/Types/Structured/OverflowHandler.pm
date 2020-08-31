package ## Hide from PAUSE
  MooseX::Types::Structured::OverflowHandler;

our $VERSION = '0.36';

use Moose;

use overload '""' => 'name', fallback => 1;

has type_constraint => (
    is       => 'ro',
    isa      => 'Moose::Meta::TypeConstraint',
    required => 1,
    handles  => [qw/check/],
);

sub name {
    my ($self) = @_;
    return 'slurpy(' . $self->type_constraint->name . ')';
}

no Moose;
__PACKAGE__->meta->make_immutable;

1;
