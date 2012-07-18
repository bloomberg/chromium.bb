package Moose::Meta::Method::Accessor::Native::Hash::accessor;
BEGIN {
  $Moose::Meta::Method::Accessor::Native::Hash::accessor::AUTHORITY = 'cpan:STEVAN';
}
{
  $Moose::Meta::Method::Accessor::Native::Hash::accessor::VERSION = '2.0602';
}

use strict;
use warnings;

use Moose::Role;

with 'Moose::Meta::Method::Accessor::Native::Hash::set' => {
    -excludes => [
        qw(
            _generate_method
            _minimum_arguments
            _maximum_arguments
            )
    ]
    },
    'Moose::Meta::Method::Accessor::Native::Hash::get' => {
    -excludes => [
        qw(
            _generate_method
            _minimum_arguments
            _maximum_arguments
            _inline_check_argument_count
            _inline_check_arguments
            _inline_process_arguments
            _return_value
            )
    ]
    };

sub _generate_method {
    my $self = shift;

    my $inv         = '$self';
    my $slot_access = $self->_get_value($inv);

    return (
        'sub {',
            'my ' . $inv . ' = shift;',
            $self->_inline_curried_arguments,
            $self->_inline_check_lazy($inv, '$type_constraint', '$type_coercion', '$type_message'),
            # get
            'if (@_ == 1) {',
                $self->_inline_check_var_is_valid_key('$_[0]'),
                $slot_access . '->{$_[0]}',
            '}',
            # set
            'else {',
                $self->_inline_writer_core($inv, $slot_access),
            '}',
        '}',
    );
}

sub _minimum_arguments { 1 }
sub _maximum_arguments { 2 }

no Moose::Role;

1;
