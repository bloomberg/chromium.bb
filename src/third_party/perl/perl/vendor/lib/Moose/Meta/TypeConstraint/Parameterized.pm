package Moose::Meta::TypeConstraint::Parameterized;
our $VERSION = '2.2011';

use strict;
use warnings;
use metaclass;

use Scalar::Util 'blessed';
use Moose::Util::TypeConstraints;
use Moose::Meta::TypeConstraint::Parameterizable;
use Moose::Util 'throw_exception';

use parent 'Moose::Meta::TypeConstraint';

__PACKAGE__->meta->add_attribute('type_parameter' => (
    accessor  => 'type_parameter',
    predicate => 'has_type_parameter',
    Class::MOP::_definition_context(),
));

__PACKAGE__->meta->add_attribute('parameterized_from' => (
    accessor   => 'parameterized_from',
    predicate  => 'has_parameterized_from',
    Class::MOP::_definition_context(),
));

sub equals {
    my ( $self, $type_or_name ) = @_;

    my $other = Moose::Util::TypeConstraints::find_type_constraint($type_or_name);

    return unless $other->isa(__PACKAGE__);

    return (
        $self->type_parameter->equals( $other->type_parameter )
            and
        $self->parent->equals( $other->parent )
    );
}

sub compile_type_constraint {
    my $self = shift;

    unless ( $self->has_type_parameter ) {
        throw_exception( CannotCreateHigherOrderTypeWithoutATypeParameter => type_name => $self->name );
    }

    my $type_parameter = $self->type_parameter;

    unless ( blessed $type_parameter && $type_parameter->isa('Moose::Meta::TypeConstraint') ) {
        throw_exception( TypeParameterMustBeMooseMetaType => type_name => $self->name );
    }

    foreach my $type (Moose::Util::TypeConstraints::get_all_parameterizable_types()) {
        if (my $constraint = $type->generate_constraint_for($self)) {
            $self->_set_constraint($constraint);
            return $self->SUPER::compile_type_constraint;
        }
    }

    # if we get here, then we couldn't
    # find a way to parameterize this type
    throw_exception( TypeConstraintCannotBeUsedForAParameterizableType => type_name        => $self->name,
                                                                          parent_type_name => $self->parent->name,
                   );
}

sub can_be_inlined {
    my $self = shift;

    return
           $self->has_parameterized_from
        && $self->parameterized_from->has_inline_generator
        && $self->type_parameter->can_be_inlined;
}

sub inline_environment {
    my $self = shift;

    return {
        ($self->has_parameterized_from
            ? (%{ $self->parameterized_from->inline_environment })
            : ()),
        ($self->has_type_parameter
            ? (%{ $self->type_parameter->inline_environment })
            : ()),
    };
}

sub _inline_check {
    my $self = shift;

    return unless $self->can_be_inlined;

    return $self->parameterized_from->generate_inline_for( $self->type_parameter, @_ );
}

sub create_child_type {
    my ($self, %opts) = @_;
    return Moose::Meta::TypeConstraint::Parameterizable->new(%opts, parent=>$self);
}

1;

# ABSTRACT: Type constraints with a bound parameter (ArrayRef[Int])

__END__

=pod

=encoding UTF-8

=head1 NAME

Moose::Meta::TypeConstraint::Parameterized - Type constraints with a bound parameter (ArrayRef[Int])

=head1 VERSION

version 2.2011

=head1 METHODS

This class is intentionally not documented because the API is
confusing and needs some work.

=head1 INHERITANCE

C<Moose::Meta::TypeConstraint::Parameterized> is a subclass of
L<Moose::Meta::TypeConstraint>.

=head1 BUGS

See L<Moose/BUGS> for details on reporting bugs.

=head1 AUTHORS

=over 4

=item *

Stevan Little <stevan.little@iinteractive.com>

=item *

Dave Rolsky <autarch@urth.org>

=item *

Jesse Luehrs <doy@tozt.net>

=item *

Shawn M Moore <code@sartak.org>

=item *

יובל קוג'מן (Yuval Kogman) <nothingmuch@woobling.org>

=item *

Karen Etheridge <ether@cpan.org>

=item *

Florian Ragwitz <rafl@debian.org>

=item *

Hans Dieter Pearcey <hdp@weftsoar.net>

=item *

Chris Prather <chris@prather.org>

=item *

Matt S Trout <mst@shadowcat.co.uk>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2006 by Infinity Interactive, Inc.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
