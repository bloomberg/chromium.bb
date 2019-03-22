package Moose::Meta::Method::Accessor::Native::Array::is_empty;
BEGIN {
  $Moose::Meta::Method::Accessor::Native::Array::is_empty::AUTHORITY = 'cpan:STEVAN';
}
{
  $Moose::Meta::Method::Accessor::Native::Array::is_empty::VERSION = '2.0602';
}

use strict;
use warnings;

use Moose::Role;

with 'Moose::Meta::Method::Accessor::Native::Reader' =>
    { -excludes => ['_maximum_arguments'] };

sub _maximum_arguments { 0 }

sub _return_value {
    my $self = shift;
    my ($slot_access) = @_;

    return '@{ (' . $slot_access . ') } ? 0 : 1';
}

no Moose::Role;

1;
