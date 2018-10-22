package Parse::Method::Signatures::Param::Placeholder;

use Moose::Role;
use namespace::clean -except => 'meta';

sub _stringify_variable_name {
    my ($self) = @_;
    return $self->sigil;
}

1;
