package MooseX::Meta::Method::Transactional::Application::ToClass;
use Moose::Role;
use Moose::Util::MetaRole;

after apply => sub {
    my ($self, $role_source, $role_dest, $args) = @_;

    Moose::Util::MetaRole::apply_base_class_roles
        (
         for   => $role_dest->name,
         roles => ['MooseX::Meta::Method::Transactional']
        );
};

1;
