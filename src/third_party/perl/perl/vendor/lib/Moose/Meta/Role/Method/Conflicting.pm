package Moose::Meta::Role::Method::Conflicting;
our $VERSION = '2.2011';

use strict;
use warnings;

use Moose::Util;

use parent 'Moose::Meta::Role::Method::Required';

__PACKAGE__->meta->add_attribute('roles' => (
    reader   => 'roles',
    required => 1,
    Class::MOP::_definition_context(),
));

sub roles_as_english_list {
    my $self = shift;
    Moose::Util::english_list( map { q{'} . $_ . q{'} } @{ $self->roles } );
}

1;

# ABSTRACT: A Moose metaclass for conflicting methods in Roles

__END__

=pod

=encoding UTF-8

=head1 NAME

Moose::Meta::Role::Method::Conflicting - A Moose metaclass for conflicting methods in Roles

=head1 VERSION

version 2.2011

=head1 DESCRIPTION

=head1 INHERITANCE

C<Moose::Meta::Role::Method::Conflicting> is a subclass of
L<Moose::Meta::Role::Method::Required>.

=head1 METHODS

=head2 Moose::Meta::Role::Method::Conflicting->new(%options)

This creates a new type constraint based on the provided C<%options>:

=over 4

=item * name

The method name. This is required.

=item * roles

The list of role names that generated the conflict. This is required.

=back

=head2 $method->name

Returns the conflicting method's name, as provided to the constructor.

=head2 $method->roles

Returns the roles that generated this conflicting method, as provided to the
constructor.

=head2 $method->roles_as_english_list

Returns the roles that generated this conflicting method as an English list.

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
