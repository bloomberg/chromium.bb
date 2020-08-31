package MooseX::Role::WithOverloading::Meta::Role::Application::ToRole;

our $VERSION = '0.17';

use Moose::Role;
use aliased 'MooseX::Role::WithOverloading::Meta::Role::Application::ToClass';
use aliased 'MooseX::Role::WithOverloading::Meta::Role::Application::ToInstance';
use namespace::autoclean;

with 'MooseX::Role::WithOverloading::Meta::Role::Application';

around apply => sub {
    my ($next, $self, $role1, $role2) = @_;
    my $new_role2 =  Moose::Util::MetaRole::apply_metaroles(
        for            => $role2,
        role_metaroles => {
            application_to_class    => [ToClass],
            application_to_role     => [__PACKAGE__],
            application_to_instance => [ToInstance],
        },
    );
    # Horrible hack as we have just got a new metaclass with no attributes
    foreach my $name ( $role2->get_attribute_list ) {
        $new_role2->add_attribute($role2->get_attribute($name));
    }

    return $self->$next($role1, $new_role2);
};

1;
