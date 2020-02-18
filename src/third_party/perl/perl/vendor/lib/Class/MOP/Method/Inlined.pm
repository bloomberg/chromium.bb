package Class::MOP::Method::Inlined;
our $VERSION = '2.2011';

use strict;
use warnings;

use Scalar::Util 'refaddr';

use parent 'Class::MOP::Method::Generated';

sub _uninlined_body {
    my $self = shift;

    my $super_method
        = $self->associated_metaclass->find_next_method_by_name( $self->name )
        or return;

    if ( $super_method->isa(__PACKAGE__) ) {
        return $super_method->_uninlined_body;
    }
    else {
        return $super_method->body;
    }
}

sub can_be_inlined {
    my $self      = shift;
    my $metaclass = $self->associated_metaclass;
    my $class     = $metaclass->name;

    # If we don't find an inherited method, this is a rather weird
    # case where we have no method in the inheritance chain even
    # though we're expecting one to be there
    my $inherited_method
        = $metaclass->find_next_method_by_name( $self->name );

    if (   $inherited_method
        && $inherited_method->isa('Class::MOP::Method::Wrapped') ) {
        warn "Not inlining '"
            . $self->name
            . "' for $class since it "
            . "has method modifiers which would be lost if it were inlined\n";

        return 0;
    }

    my $expected_class = $self->_expected_method_class
        or return 1;

    # if we are shadowing a method we first verify that it is
    # compatible with the definition we are replacing it with
    my $expected_method = $expected_class->can( $self->name );

    if ( ! $expected_method ) {
        warn "Not inlining '"
            . $self->name
            . "' for $class since ${expected_class}::"
            . $self->name
            . " is not defined\n";

        return 0;
    }

    my $actual_method = $class->can( $self->name )
        or return 1;

    # the method is what we wanted (probably Moose::Object::new)
    return 1
        if refaddr($expected_method) == refaddr($actual_method);

    # otherwise we have to check that the actual method is an inlined
    # version of what we're expecting
    if ( $inherited_method->isa(__PACKAGE__) ) {
        if ( $inherited_method->_uninlined_body
             && refaddr( $inherited_method->_uninlined_body )
             == refaddr($expected_method) ) {
            return 1;
        }
    }
    elsif ( refaddr( $inherited_method->body )
            == refaddr($expected_method) ) {
        return 1;
    }

    my $warning
        = "Not inlining '"
        . $self->name
        . "' for $class since it is not"
        . " inheriting the default ${expected_class}::"
        . $self->name . "\n";

    if ( $self->isa("Class::MOP::Method::Constructor") ) {

        # FIXME kludge, refactor warning generation to a method
        $warning
            .= "If you are certain you don't need to inline your"
            . " constructor, specify inline_constructor => 0 in your"
            . " call to $class->meta->make_immutable\n";
    }

    warn $warning;

    return 0;
}

1;

# ABSTRACT: Method base class for methods which have been inlined

__END__

=pod

=encoding UTF-8

=head1 NAME

Class::MOP::Method::Inlined - Method base class for methods which have been inlined

=head1 VERSION

version 2.2011

=head1 DESCRIPTION

This is a L<Class::MOP::Method::Generated> subclass for methods which
can be inlined.

=head1 METHODS

=head2 $metamethod->can_be_inlined

This method returns true if the method in question can be inlined in
the associated metaclass.

If it cannot be inlined, it spits out a warning and returns false.

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
