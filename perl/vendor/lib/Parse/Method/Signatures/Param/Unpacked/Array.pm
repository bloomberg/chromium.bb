package Parse::Method::Signatures::Param::Unpacked::Array;

use Moose::Role;
use namespace::clean -except => 'meta';

with 'Parse::Method::Signatures::Param::Unpacked';

sub _stringify_variable_name {
    my ($self) = @_;
    return '[' . $self->_params->to_string . ']';
}

1;
