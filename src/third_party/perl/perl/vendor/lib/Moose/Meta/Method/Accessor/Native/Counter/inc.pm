package Moose::Meta::Method::Accessor::Native::Counter::inc;
BEGIN {
  $Moose::Meta::Method::Accessor::Native::Counter::inc::AUTHORITY = 'cpan:STEVAN';
}
{
  $Moose::Meta::Method::Accessor::Native::Counter::inc::VERSION = '2.0602';
}

use strict;
use warnings;

use Moose::Role;

with 'Moose::Meta::Method::Accessor::Native::Writer' => {
    -excludes => [
        qw(
            _minimum_arguments
            _maximum_arguments
            _inline_optimized_set_new_value
            )
    ]
};

sub _minimum_arguments { 0 }
sub _maximum_arguments { 1 }

sub _potential_value {
    my $self = shift;
    my ($slot_access) = @_;

    return $slot_access . ' + (defined $_[0] ? $_[0] : 1)';
}

sub _inline_optimized_set_new_value {
    my $self = shift;
    my ($inv, $new, $slot_access) = @_;

    return $slot_access . ' += defined $_[0] ? $_[0] : 1;';
}

no Moose::Role;

1;
