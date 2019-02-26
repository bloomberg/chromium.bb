package Parse::Method::Signatures::Param::Positional;

use Moose::Role;
use namespace::clean -except => 'meta';

sub _stringify_required {
    my ($self) = @_;
    return $self->required ? q{} : q{?};
}

1;
