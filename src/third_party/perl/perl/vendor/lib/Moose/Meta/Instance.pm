package Moose::Meta::Instance;
our $VERSION = '2.2011';

use strict;
use warnings;

use Class::MOP::MiniTrait;

use parent 'Class::MOP::Instance';

Class::MOP::MiniTrait::apply(__PACKAGE__, 'Moose::Meta::Object::Trait');

1;

# ABSTRACT: The Moose Instance metaclass

__END__

=pod

=encoding UTF-8

=head1 NAME

Moose::Meta::Instance - The Moose Instance metaclass

=head1 VERSION

version 2.2011

=head1 SYNOPSIS

    # nothing to see here

=head1 DESCRIPTION

This class provides the low level data storage abstractions for
attributes.

Using this API directly in your own code violates encapsulation, and
we recommend that you use the appropriate APIs in
L<Moose::Meta::Class> and L<Moose::Meta::Attribute> instead. Those
APIs in turn call the methods in this class as appropriate.

At present, this is an empty subclass of L<Class::MOP::Instance>, so
you should see that class for all API details.

=head1 INHERITANCE

C<Moose::Meta::Instance> is a subclass of L<Class::MOP::Instance>.

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
