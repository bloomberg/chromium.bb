package # hide from PAUSE
    MooseX::Declare::StackItem;

our $VERSION = '0.43';

use Moose;

use namespace::autoclean;
use overload '""' => 'as_string', fallback => 1;

has identifier => (
    is          => 'rw',
    isa         => 'Str',
    required    => 1,
);

has handler => (
    is          => 'ro',
    required    => 1,
    default     => '',
);

has is_dirty => (
    is          => 'ro',
    isa         => 'Bool',
);

has is_parameterized => (
    is  => 'ro',
    isa => 'Bool',
);

has namespace => (
    is          => 'ro',
    isa         => 'Str|Undef',

);

sub as_string {
    my ($self) = @_;
    return $self->identifier;
}

sub serialize {
    my ($self) = @_;
    return sprintf '%s->new(%s)',
        ref($self),
        join ', ', map { defined($_) ? "q($_)" : 'undef' }
        'identifier',       $self->identifier,
        'handler',          $self->handler,
        'is_dirty',         ( $self->is_dirty         ? 1 : 0 ),
        'is_parameterized', ( $self->is_parameterized ? 1 : 0 ),
        'namespace',        $self->namespace,
        ;
}

__PACKAGE__->meta->make_immutable;

1;
