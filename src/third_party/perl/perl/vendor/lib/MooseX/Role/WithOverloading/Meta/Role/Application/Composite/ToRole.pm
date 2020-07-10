package MooseX::Role::WithOverloading::Meta::Role::Application::Composite::ToRole;

our $VERSION = '0.17';

use Moose::Role;
use namespace::autoclean;

with qw(
    MooseX::Role::WithOverloading::Meta::Role::Application::Composite
    MooseX::Role::WithOverloading::Meta::Role::Application::ToRole
);

1;
