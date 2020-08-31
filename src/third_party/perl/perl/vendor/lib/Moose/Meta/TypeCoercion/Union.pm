package Moose::Meta::TypeCoercion::Union;
our $VERSION = '2.2011';

use strict;
use warnings;
use metaclass;

use Scalar::Util 'blessed';

use parent 'Moose::Meta::TypeCoercion';

use Moose::Util 'throw_exception';

sub compile_type_coercion {
    my $self            = shift;
    my $type_constraint = $self->type_constraint;

    (blessed $type_constraint && $type_constraint->isa('Moose::Meta::TypeConstraint::Union'))
     || throw_exception( NeedsTypeConstraintUnionForTypeCoercionUnion => type_coercion_union_object => $self,
                                                                         type_name                  => $type_constraint->name
                       );

    $self->_compiled_type_coercion(
        sub {
            my $value = shift;

            foreach my $type ( grep { $_->has_coercion }
                @{ $type_constraint->type_constraints } ) {
                my $temp = $type->coerce($value);
                return $temp if $type_constraint->check($temp);
            }

            return $value;
        }
    );
}

sub has_coercion_for_type { 0 }

sub add_type_coercions {
    my $self = shift;
    throw_exception( CannotAddAdditionalTypeCoercionsToUnion => type_coercion_union_object => $self );
}

1;

# ABSTRACT: The Moose Type Coercion metaclass for Unions

__END__

=pod

=encoding UTF-8

=head1 NAME

Moose::Meta::TypeCoercion::Union - The Moose Type Coercion metaclass for Unions

=head1 VERSION

version 2.2011

=head1 DESCRIPTION

This is a subclass of L<Moose::Meta::TypeCoercion> that is used for
L<Moose::Meta::TypeConstraint::Union> objects.

=head1 METHODS

=head2 $coercion->has_coercion_for_type

This method always returns false.

=head2 $coercion->add_type_coercions

This method always throws an error. You cannot add coercions to a
union type coercion.

=head2 $coercion->coerce($value)

This method will coerce by trying the coercions for each type in the
union.

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
