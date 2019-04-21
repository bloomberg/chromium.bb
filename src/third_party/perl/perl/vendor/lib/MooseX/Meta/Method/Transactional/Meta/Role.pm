package MooseX::Meta::Method::Transactional::Meta::Role;
use Moose::Role;
use Moose::Util::MetaRole;
use Moose::Exporter;
Moose::Exporter->setup_import_methods(also => 'Moose::Role');

sub init_meta {
    my ($class, %opts) = @_;

    Moose::Role->init_meta(%opts);

    Moose::Util::MetaRole::apply_metaroles
      (
       for            => $opts{for_class},
       role_metaroles =>
       {
        application_to_role =>
        ['MooseX::Meta::Method::Transactional::Application::ToComposite'],
        application_to_class =>
        ['MooseX::Meta::Method::Transactional::Application::ToClass'],
        application_to_instance    =>
        ['MooseX::Meta::Method::Transactional::Application::ToInstance'],
       }
      );

    return $opts{for_class}->meta();
};


1;
