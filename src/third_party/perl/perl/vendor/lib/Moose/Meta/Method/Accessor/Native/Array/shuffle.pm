package Moose::Meta::Method::Accessor::Native::Array::shuffle;
BEGIN {
  $Moose::Meta::Method::Accessor::Native::Array::shuffle::AUTHORITY = 'cpan:STEVAN';
}
{
  $Moose::Meta::Method::Accessor::Native::Array::shuffle::VERSION = '2.0602';
}

use strict;
use warnings;

use List::Util ();

use Moose::Role;

with 'Moose::Meta::Method::Accessor::Native::Reader' =>
    { -excludes => ['_maximum_arguments'] };

sub _maximum_arguments { 0 }

sub _return_value {
    my $self = shift;
    my ($slot_access) = @_;

    return 'List::Util::shuffle @{ (' . $slot_access . ') }';
}

no Moose::Role;

1;
