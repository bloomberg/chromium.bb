package Moose::Meta::Role::Method::Required;
our $VERSION = '2.2011';

use strict;
use warnings;
use metaclass;

use overload
    '""' => sub { shift->name },   # stringify to method name
    'bool' => sub { 1 },
    fallback => 1;

use parent 'Class::MOP::Object';

# This is not a Moose::Meta::Role::Method because it has no implementation, it
# is just a name

__PACKAGE__->meta->add_attribute('name' => (
    reader   => 'name',
    required => 1,
    Class::MOP::_definition_context(),
));

sub new { shift->_new(@_) }

1;

# ABSTRACT: A Moose metaclass for required methods in Roles

__END__

=pod

=encoding UTF-8

=head1 NAME

Moose::Meta::Role::Method::Required - A Moose metaclass for required methods in Roles

=head1 VERSION

version 2.2011

=head1 DESCRIPTION

=head1 INHERITANCE

C<Moose::Meta::Role::Method::Required> is a subclass of L<Class::MOP::Object>.
It is B<not> a subclass of C<Moose::Meta::Role::Method> since it does not
provide an implementation of the method.

=head1 METHODS

=head2 Moose::Meta::Role::Method::Required->new(%options)

This creates a new type constraint based on the provided C<%options>:

=over 4

=item * name

The method name. This is required.

=back

=head2 $method->name

Returns the required method's name, as provided to the constructor.

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
