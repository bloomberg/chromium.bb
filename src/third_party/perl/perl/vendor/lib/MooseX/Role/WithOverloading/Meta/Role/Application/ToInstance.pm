package MooseX::Role::WithOverloading::Meta::Role::Application::ToInstance;

our $VERSION = '0.17';

use Moose::Role;
use namespace::autoclean;

with qw(
    MooseX::Role::WithOverloading::Meta::Role::Application
    MooseX::Role::WithOverloading::Meta::Role::Application::FixOverloadedRefs
);

1;
