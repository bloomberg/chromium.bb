package Moose::Meta::Method::Overridden;
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
    my $super_package = $args{package} || $args{class}->name;

    my $name = $args{name};

    my $super = $args{class}->find_next_method_by_name($name);

    (defined $super)
        || throw_exception( CannotOverrideNoSuperMethod => class       => $class,
                                                           params      => \%args,
                                                           method_name => $name
                          );

    my $super_body = $super->body;

    my $method = $args{method};

    my $body = sub {
        local $Moose::SUPER_PACKAGE = $super_package;
        local @Moose::SUPER_ARGS = @_;
        local $Moose::SUPER_BODY = $super_body;
        return $method->(@_);
    };

    # FIXME do we need this make sure this works for next::method?
    # subname "${super_package}::${name}", $method;

    # FIXME store additional attrs
    $class->wrap(
        $body,
        package_name => $args{class}->name,
        name         => $name
    );
}

1;

# ABSTRACT: A Moose Method metaclass for overridden methods

__END__

=pod

=encoding UTF-8

=head1 NAME

Moose::Meta::Method::Overridden - A Moose Method metaclass for overridden methods

=head1 VERSION

version 2.2011

=head1 DESCRIPTION

This class implements method overriding logic for the L<Moose>
C<override> keyword.

The overriding subroutine's parent will be invoked explicitly using
the C<super> keyword from the parent class's method definition.

=head1 METHODS

=head2 Moose::Meta::Method::Overridden->new(%options)

This constructs a new object. It accepts the following options:

=over 4

=item * class

The metaclass object for the class in which the override is being
declared. This option is required.

=item * name

The name of the method which we are overriding. This method must exist
in one of the class's superclasses. This option is required.

=item * method

The subroutine reference which implements the overriding. This option
is required.

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
