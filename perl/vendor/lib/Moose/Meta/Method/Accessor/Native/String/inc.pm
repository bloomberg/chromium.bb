package Moose::Meta::Method::Accessor::Native::String::inc;
BEGIN {
  $Moose::Meta::Method::Accessor::Native::String::inc::AUTHORITY = 'cpan:STEVAN';
}
{
  $Moose::Meta::Method::Accessor::Native::String::inc::VERSION = '2.0602';
}

use strict;
use warnings;

use Moose::Role;

with 'Moose::Meta::Method::Accessor::Native::Writer' => {
    -excludes => [
        qw(
            _maximum_arguments
            _inline_optimized_set_new_value
            )
    ]
    };

sub _maximum_arguments { 0 }

sub _potential_value {
    my $self = shift;
    my ($slot_access) = @_;

    return '(do { '
             . 'my $val = ' . $slot_access . '; '
             . '$val++; '
             . '$val; '
         . '})';
}

sub _inline_optimized_set_new_value {
    my $self = shift;
    my ($inv, $new, $slot_access) = @_;

    return $slot_access . '++;';
}

no Moose::Role;

1;
