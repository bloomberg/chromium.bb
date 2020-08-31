package oose;
our $VERSION = '2.2011';

use strict;
use warnings;

use Moose::Util ();

BEGIN {
    my $package;
    sub import {
        $package = $_[1] || 'Class';
        if ($package =~ /^\+/) {
            $package =~ s/^\+//;
            Moose::Util::_load_user_class($package);
        }
    }
    use Filter::Simple sub { s/^/package $package;\nuse Moose;use Moose::Util::TypeConstraints;\n/; }
}

1;

# ABSTRACT: syntactic sugar to make Moose one-liners easier

__END__

=pod

=encoding UTF-8

=head1 NAME

oose - syntactic sugar to make Moose one-liners easier

=head1 VERSION

version 2.2011

=head1 SYNOPSIS

  # create a Moose class on the fly ...
  perl -Moose=Foo -e 'has bar => ( is=>q[ro], default => q[baz] ); print Foo->new->bar' # prints baz

  # loads an existing class (Moose or non-Moose)
  # and re-"opens" the package definition to make
  # debugging/introspection easier
  perl -Moose=+My::Class -e 'print join ", " => __PACKAGE__->meta->get_method_list'

  # also loads Moose::Util::TypeConstraints to allow subtypes etc
  perl -Moose=Person -e'subtype q[ValidAge] => as q[Int] => where { $_ > 0 && $_ < 78 }; has => age ( isa => q[ValidAge], is => q[ro]); Person->new(age => 90)'

=head1 DESCRIPTION

oose.pm is a simple source filter that adds
C<package $name; use Moose; use Moose::Util::TypeConstraints;>
to the beginning of your script and was entirely created because typing
C<perl -e'package Foo; use Moose; ...'> was annoying me.

=head1 INTERFACE

oose provides exactly one method and it's automatically called by perl:

=over 4

=item B<import($package)>

Pass a package name to import to be used by the source filter. The
package defaults to C<Class> if none is given.

=back

=head1 DEPENDENCIES

You will need L<Filter::Simple> and eventually L<Moose>

=head1 INCOMPATIBILITIES

None reported. But it is a source filter and might have issues there.

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
