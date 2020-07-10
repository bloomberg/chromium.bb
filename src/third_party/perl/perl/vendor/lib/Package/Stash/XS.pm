package Package::Stash::XS; # git description: 4627d81
use strict;
use warnings;
use 5.008001;
# ABSTRACT: faster and more correct implementation of the Package::Stash API

our $VERSION = '0.29';

use XSLoader;
XSLoader::load(
    __PACKAGE__,
    # we need to be careful not to touch $VERSION at compile time, otherwise
    # DynaLoader will assume it's set and check against it, which will cause
    # fail when being run in the checkout without dzil having set the actual
    # $VERSION
    exists $Package::Stash::XS::{VERSION}
        ? ${ $Package::Stash::XS::{VERSION} } : (),
);


1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Package::Stash::XS - faster and more correct implementation of the Package::Stash API

=head1 VERSION

version 0.29

=head1 SYNOPSIS

  use Package::Stash;

=head1 DESCRIPTION

This is a backend for L<Package::Stash>, which provides the functionality in a
way that's less buggy and much faster. It will be used by default if it's
installed, and should be preferred in all environments with a compiler.

=head1 SEE ALSO

L<Package::Stash>

=head1 SUPPORT

You can find this documentation for this module with the perldoc command.

    perldoc Package::Stash::XS

You can also look for information at:

=over 4

=item * MetaCPAN

L<https://metacpan.org/release/Package-Stash-XS>

=item * Github

L<https://github.com/moose/Package-Stash-XS>

=item * RT: CPAN's request tracker

L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=Package-Stash-XS>

=item * CPAN Ratings

L<http://cpanratings.perl.org/d/Package-Stash-XS>

=back

=for Pod::Coverage add_symbol
get_all_symbols
get_or_add_symbol
get_symbol
has_symbol
list_all_symbols
name
namespace
new
remove_glob
remove_symbol

=head1 BUGS

No known bugs (but see the BUGS section in L<Package::Stash>).

Bugs may be submitted through L<the RT bug tracker|https://rt.cpan.org/Public/Dist/Display.html?Name=Package-Stash-XS>
(or L<bug-Package-Stash-XS@rt.cpan.org|mailto:bug-Package-Stash-XS@rt.cpan.org>).

=head1 AUTHOR

Jesse Luehrs <doy@tozt.net>

=head1 CONTRIBUTORS

=for stopwords Florian Ragwitz Karen Etheridge Dave Rolsky Justin Hunter Tim Bunce

=over 4

=item *

Florian Ragwitz <rafl@debian.org>

=item *

Karen Etheridge <ether@cpan.org>

=item *

Dave Rolsky <autarch@urth.org>

=item *

Justin Hunter <justin.d.hunter@gmail.com>

=item *

Tim Bunce <Tim.Bunce@pobox.com>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2018 by Jesse Luehrs.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
