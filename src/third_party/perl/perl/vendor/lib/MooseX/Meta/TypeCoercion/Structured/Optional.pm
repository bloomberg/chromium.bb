package ## Hide from PAUSE
  MooseX::Meta::TypeCoercion::Structured::Optional;

our $VERSION = '0.36';

use Moose;
extends 'Moose::Meta::TypeCoercion';

sub compile_type_coercion {
    my ($self) = @_;
    my $constraint = $self->type_constraint->type_parameter;

    $self->_compiled_type_coercion(sub {
        my ($value) = @_;
        return unless $constraint->has_coercion;
        return $constraint->coerce($value);
    });
}

sub has_coercion_for_type { 0 }

sub add_type_coercions {
    Moose->throw_error("Cannot add additional type coercions to Optional types");
}

no Moose;
__PACKAGE__->meta->make_immutable(inline_constructor => 0);

1;
