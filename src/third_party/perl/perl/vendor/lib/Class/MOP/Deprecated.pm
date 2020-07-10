package Class::MOP::Deprecated;
our $VERSION = '2.2011';

use strict;
use warnings;

use Package::DeprecationManager -deprecations => {
    'Class::Load wrapper functions' => '2.1100',
};

1;

# ABSTRACT: Manages deprecation warnings for Class::MOP

__END__

=pod

=encoding UTF-8

=head1 NAME

Class::MOP::Deprecated - Manages deprecation warnings for Class::MOP

=head1 VERSION

version 2.2011

=head1 DESCRIPTION

    use Class::MOP::Deprecated -api_version => $version;

=head1 FUNCTIONS

This module manages deprecation warnings for features that have been
deprecated in Class::MOP.

If you specify C<< -api_version => $version >>, you can use deprecated features
without warnings. Note that this special treatment is limited to the package
that loads C<Class::MOP::Deprecated>.

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
