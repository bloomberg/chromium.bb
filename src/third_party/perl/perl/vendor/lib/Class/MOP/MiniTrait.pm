package Class::MOP::MiniTrait;
our $VERSION = '2.2011';

use strict;
use warnings;

use Module::Runtime 'use_package_optimistically';

sub apply {
    my ( $to_class, $trait ) = @_;

    for ( grep { !ref } $to_class, $trait ) {
        use_package_optimistically($_);
        $_ = Class::MOP::Class->initialize($_);
    }

    for my $meth ( grep { $_->package_name ne 'UNIVERSAL' } $trait->get_all_methods ) {
        my $meth_name = $meth->name;
        next if index($meth_name, '__') == 0;   # skip private subs

        if ( $to_class->find_method_by_name($meth_name) ) {
            $to_class->add_around_method_modifier( $meth_name, $meth->body );
        }
        else {
            $to_class->add_method( $meth_name, $meth->clone );
        }
    }
}

# We can't load this with use, since it may be loaded and used from Class::MOP
# (via Class::MOP::Class, etc). However, if for some reason this module is loaded
# _without_ first loading Class::MOP we need to require Class::MOP so we can
# use it and Class::MOP::Class.
require Class::MOP;

1;

# ABSTRACT: Extremely limited trait application

__END__

=pod

=encoding UTF-8

=head1 NAME

Class::MOP::MiniTrait - Extremely limited trait application

=head1 VERSION

version 2.2011

=head1 DESCRIPTION

This package provides a single function, C<apply>, which does a half-assed job
of applying a trait to a class. It exists solely for use inside Class::MOP and
L<Moose> core classes.

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
