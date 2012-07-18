package Moose::Meta::Method::Accessor::Native::String::match;
BEGIN {
  $Moose::Meta::Method::Accessor::Native::String::match::AUTHORITY = 'cpan:STEVAN';
}
{
  $Moose::Meta::Method::Accessor::Native::String::match::VERSION = '2.0602';
}

use strict;
use warnings;

use Moose::Util ();
use Params::Util ();

use Moose::Role;

with 'Moose::Meta::Method::Accessor::Native::Reader' => {
    -excludes => [
        qw(
            _minimum_arguments
            _maximum_arguments
            _inline_check_arguments
            )
    ]
};

sub _minimum_arguments { 1 }

sub _maximum_arguments { 1 }

sub _inline_check_arguments {
    my $self = shift;

    return (
        'if (!Moose::Util::_STRINGLIKE0($_[0]) && !Params::Util::_REGEX($_[0])) {',
            $self->_inline_throw_error(
                '"The argument passed to match must be a string or regexp '
              . 'reference"',
            ) . ';',
        '}',
    );
}

sub _return_value {
    my $self = shift;
    my ($slot_access) = @_;

    return $slot_access . ' =~ $_[0]';
}

no Moose::Role;

1;
