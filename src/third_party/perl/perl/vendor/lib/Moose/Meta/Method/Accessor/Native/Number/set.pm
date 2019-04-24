package Moose::Meta::Method::Accessor::Native::Number::set;
BEGIN {
  $Moose::Meta::Method::Accessor::Native::Number::set::AUTHORITY = 'cpan:STEVAN';
}
{
  $Moose::Meta::Method::Accessor::Native::Number::set::VERSION = '2.0602';
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

sub _minimum_arguments { 1 }
sub _maximum_arguments { 1 }

sub _potential_value { '$_[0]' }

sub _inline_optimized_set_new_value {
    my $self = shift;
    my ($inv, $new, $slot_access) = @_;

    return $slot_access . ' = $_[0];';
}

no Moose::Role;

1;
