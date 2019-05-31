package MooseX::Role::WithOverloading::Meta::Role::Composite;

our $VERSION = '0.17';

use Moose::Role;
use Moose::Util::MetaRole;
use aliased 'MooseX::Role::WithOverloading::Meta::Role::Application::Composite::ToClass';
use aliased 'MooseX::Role::WithOverloading::Meta::Role::Application::Composite::ToRole';
use aliased 'MooseX::Role::WithOverloading::Meta::Role::Application::Composite::ToInstance';

use namespace::autoclean;

around apply_params => sub {
    my ($next, $self, @args) = @_;
    return Moose::Util::MetaRole::apply_metaroles(
        for            => $self->$next(@args),
        role_metaroles => {
            application_to_class    => [ToClass],
            application_to_role     => [ToRole],
            application_to_instance => [ToInstance],
        },
    );
};

1;
