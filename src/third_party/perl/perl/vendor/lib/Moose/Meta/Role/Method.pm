package Moose::Meta::Role::Method;
our $VERSION = '2.2011';

use strict;
use warnings;

use parent 'Moose::Meta::Method';

sub _make_compatible_with {
    my $self = shift;
    my ($other) = @_;

    # XXX: this is pretty gross. the issue here is blah blah blah
    # see the comments in CMOP::Method::Meta and CMOP::Method::Wrapped
    return $self unless $other->_is_compatible_with($self->_real_ref_name);

    return $self->SUPER::_make_compatible_with(@_);
}

1;

# ABSTRACT: A Moose Method metaclass for Roles

__END__

=pod

=encoding UTF-8

=head1 NAME

Moose::Meta::Role::Method - A Moose Method metaclass for Roles

=head1 VERSION

version 2.2011

=head1 DESCRIPTION

This is primarily used to mark methods coming from a role
as being different. Right now it is nothing but a subclass
of L<Moose::Meta::Method>.

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
