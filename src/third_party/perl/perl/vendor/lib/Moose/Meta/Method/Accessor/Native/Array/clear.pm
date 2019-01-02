package Moose::Meta::Method::Accessor::Native::Array::clear;
BEGIN {
  $Moose::Meta::Method::Accessor::Native::Array::clear::AUTHORITY = 'cpan:STEVAN';
}
{
  $Moose::Meta::Method::Accessor::Native::Array::clear::VERSION = '2.0602';
}

use strict;
use warnings;

use Moose::Role;

with 'Moose::Meta::Method::Accessor::Native::Array::Writer' => {
    -excludes => [
        qw(
            _maximum_arguments
            _inline_optimized_set_new_value
            _return_value
            )
    ]
};

sub _maximum_arguments { 0 }

sub _adds_members { 0 }

sub _potential_value { '[]' }

sub _inline_optimized_set_new_value {
    my $self = shift;
    my ($inv, $new, $slot_access) = @_;

    return $slot_access . ' = [];';
}

sub _return_value { '' }

no Moose::Role;

1;
