package Class::Load::XS; # git description: v0.09-19-g9364900

use strict;
use warnings;

our $VERSION = '0.10';

use Class::Load 0.20;

use XSLoader;
XSLoader::load(
    __PACKAGE__,
    $VERSION,
);

1;

# ABSTRACT: XS implementation of parts of Class::Load
# KEYWORDS: class module load require use runtime XS

__END__

=pod

=encoding UTF-8

=head1 NAME

Class::Load::XS - XS implementation of parts of Class::Load

=head1 VERSION

version 0.10

=head1 SYNOPSIS

    use Class::Load;

=head1 DESCRIPTION

This module provides an XS implementation for portions of L<Class::Load>. See
L<Class::Load> for API details.

=for Pod::Coverage is_class_loaded

=head1 SUPPORT

Bugs may be submitted through L<the RT bug tracker|https://rt.cpan.org/Public/Dist/Display.html?Name=Class-Load-XS>
(or L<bug-Class-Load-XS@rt.cpan.org|mailto:bug-Class-Load-XS@rt.cpan.org>).

There is also a mailing list available for users of this distribution, at
L<http://lists.perl.org/list/moose.html>.

There is also an irc channel available for users of this distribution, at
L<C<#moose> on C<irc.perl.org>|irc://irc.perl.org/#moose>.

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 CONTRIBUTORS

=for stopwords Karen Etheridge Jesse Luehrs hurricup

=over 4

=item *

Karen Etheridge <ether@cpan.org>

=item *

Jesse Luehrs <doy@tozt.net>

=item *

hurricup <hurricup@gmail.com>

=back

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2011 by Dave Rolsky.

This is free software, licensed under:

  The Artistic License 2.0 (GPL Compatible)

=cut
