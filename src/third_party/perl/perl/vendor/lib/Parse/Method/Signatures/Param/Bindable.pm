package Parse::Method::Signatures::Param::Bindable;

use Moose::Role;
use Parse::Method::Signatures::Types qw/VariableName/;

use namespace::clean -except => 'meta';

has variable_name => (
    is => 'ro',
    isa => VariableName,
    required => 1,
);

sub _stringify_variable_name {
    my ($self) = @_;
    return $self->variable_name;
}

1;
