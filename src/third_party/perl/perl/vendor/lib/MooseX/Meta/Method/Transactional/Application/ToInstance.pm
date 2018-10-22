package MooseX::Meta::Method::Transactional::Application::ToInstance;
use Moose::Role;

after apply => sub {
    my ($self, $role, $instance, $args) = @_;

    $instance->meta->add_role($role);

    my $original_body = $instance->body;
    my $new_body = sub {
        my ($self) = @_;
        $instance->schema->($self)->txn_do($original_body, @_);
    };

    # HACK! XXX!

    # 1 - body is ro in Method, need to force the change,,,
    $instance->{body} = $new_body;

    # 2 - need to reinstall the CODE ref in the glob
    no warnings 'redefine';
    no strict 'refs';
    *{$instance->package_name.'::'.$instance->name} = $new_body;
};

1;
