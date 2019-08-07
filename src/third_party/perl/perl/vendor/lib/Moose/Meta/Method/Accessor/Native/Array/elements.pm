package Moose::Meta::Method::Accessor::Native::Array::elements;
BEGIN {
  $Moose::Meta::Method::Accessor::Native::Array::elements::AUTHORITY = 'cpan:STEVAN';
}
{
  $Moose::Meta::Method::Accessor::Native::Array::elements::VERSION = '2.0602';
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

    return '@{ (' . $slot_access . ') }';
}

no Moose::Role;

1;
