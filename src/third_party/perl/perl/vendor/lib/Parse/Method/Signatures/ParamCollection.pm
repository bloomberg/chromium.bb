package Parse::Method::Signatures::ParamCollection;

use Moose;
use MooseX::Types::Moose qw/ArrayRef/;
use Parse::Method::Signatures::Types qw/Param/;

use namespace::clean -except => 'meta';

has params => (
    is         => 'ro',
    isa        => ArrayRef[Param],
    required   => 1,
    auto_deref => 1,
);

sub to_string {
    my ($self) = @_;
    return join(q{, }, map { $_->to_string } $self->params);
}

1;
