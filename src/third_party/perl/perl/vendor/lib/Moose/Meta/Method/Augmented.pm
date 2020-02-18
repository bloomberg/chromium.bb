package Moose::Meta::Method::Augmented;
our $VERSION = '2.2011';

use strict;
use warnings;

use parent 'Moose::Meta::Method';

use Moose::Util 'throw_exception';

sub new {
    my ( $class, %args ) = @_;

    # the package can be overridden by roles
    # it is really more like body's compilation stash
    # this is where we need to override the definition of super() so that the
    # body of the code can call the right overridden version
    my $name = $args{name};
    my $meta = $args{class};

    my $super = $meta->find_next_method_by_name($name);

    (defined $super)
        || throw_exception( CannotAugmentNoSuperMethod => params      => \%args,
                                                          class       => $class,
                                                          method_name => $name
                          );

    my $_super_package = $super->package_name;
    # BUT!,... if this is an overridden method ....
    if ($super->isa('Moose::Meta::Method::Overridden')) {
        # we need to be sure that we actually
        # find the next method, which is not
        # an 'override' method, the reason is
        # that an 'override' method will not
        # be the one calling inner()
        my $real_super = $meta->_find_next_method_by_name_which_is_not_overridden($name);
        $_super_package = $real_super->package_name;
    }

    my $super_body = $super->body;

    my $method = $args{method};

    my $body = sub {
        local $Moose::INNER_ARGS{$_super_package} = [ @_ ];
        local $Moose::INNER_BODY{$_super_package} = $method;
        $super_body->(@_);
    };

    # FIXME store additional attrs
    $class->wrap(
        $body,
        package_name => $meta->name,
        name         => $name
    );
}

1;

# ABSTRACT: A Moose Method metaclass for augmented methods

__END__

=pod

=encoding UTF-8

=head1 NAME

Moose::Meta::Method::Augmented - A Moose Method metaclass for augmented methods

=head1 VERSION

version 2.2011

=head1 DESCRIPTION

This class implements method augmentation logic for the L<Moose>
C<augment> keyword.

The augmentation subroutine reference will be invoked explicitly using
the C<inner> keyword from the parent class's method definition.

=head1 INHERITANCE

C<Moose::Meta::Method::Augmented> is a subclass of L<Moose::Meta::Method>.

=head1 METHODS

=head2 Moose::Meta::Method::Augmented->new(%options)

This constructs a new object. It accepts the following options:

=over 4

=item * class

The metaclass object for the class in which the augmentation is being
declared. This option is required.

=item * name

The name of the method which we are augmenting. This method must exist
in one of the class's superclasses. This option is required.

=item * method

The subroutine reference which implements the augmentation. This
option is required.

=back

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
