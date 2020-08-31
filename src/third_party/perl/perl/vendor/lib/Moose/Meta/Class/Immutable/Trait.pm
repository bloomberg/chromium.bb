package Moose::Meta::Class::Immutable::Trait;
our $VERSION = '2.2011';

use strict;
use warnings;

use Class::MOP;
use Scalar::Util qw( blessed );

use parent 'Class::MOP::Class::Immutable::Trait';

use Moose::Util 'throw_exception';

sub add_role { $_[1]->_immutable_cannot_call }

sub calculate_all_roles {
    my $orig = shift;
    my $self = shift;
    @{ $self->{__immutable}{calculate_all_roles} ||= [ $self->$orig ] };
}

sub calculate_all_roles_with_inheritance {
    my $orig = shift;
    my $self = shift;
    @{ $self->{__immutable}{calculate_all_roles_with_inheritance} ||= [ $self->$orig ] };
}

sub does_role {
    shift;
    my $self = shift;
    my $role = shift;

    (defined $role)
        || throw_exception( RoleNameRequired => class_name => $self->name );

    $self->{__immutable}{does_role} ||= { map { $_->name => 1 } $self->calculate_all_roles_with_inheritance };

    my $name = blessed $role ? $role->name : $role;

    return $self->{__immutable}{does_role}{$name};
}

1;

# ABSTRACT: Implements immutability for metaclass objects

__END__

=pod

=encoding UTF-8

=head1 NAME

Moose::Meta::Class::Immutable::Trait - Implements immutability for metaclass objects

=head1 VERSION

version 2.2011

=head1 DESCRIPTION

This class makes some Moose-specific metaclass methods immutable. This
is deep guts.

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
