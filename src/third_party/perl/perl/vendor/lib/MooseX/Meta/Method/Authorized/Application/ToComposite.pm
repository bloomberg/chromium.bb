package MooseX::Meta::Method::Authorized::Application::ToComposite;
use Moose::Role;

after apply => sub {
    my ($self, $role_source, $role_dest, $args) = @_;

    Moose::Util::MetaRole::apply_metaroles
        (
         for            => $role_dest,
         role_metaroles =>
         {
          application_to_role =>
          ['MooseX::Meta::Method::Authorized::Application::ToComposite'],
          application_to_class =>
          ['MooseX::Meta::Method::Authorized::Application::ToClass'],
          application_to_instance    =>
          ['MooseX::Meta::Method::Authorized::Application::ToInstance'],
         }
        );

};

1;
