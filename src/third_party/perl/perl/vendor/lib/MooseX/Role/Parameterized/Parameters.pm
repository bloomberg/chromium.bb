package MooseX::Role::Parameterized::Parameters;
use Moose;

our $VERSION = '0.27';

__PACKAGE__->meta->make_immutable;
no Moose;

1;

__END__

=head1 NAME

MooseX::Role::Parameterized::Parameters - base class for parameters

=head1 DESCRIPTION

This is the base class for parameter objects. Currently empty, but I reserve
the right to add things here.

Each parameterizable role gets their own anonymous subclass of this;
L<MooseX::Role::Parameterized/parameter> actually operates on these anonymous
subclasses.

Each parameterized role gets their own instance of the anonymous subclass
(owned by the parameterizable role).

=cut

