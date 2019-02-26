package Class::Load::XS;
{
  $Class::Load::XS::VERSION = '0.04';
}

use strict;
use warnings;

use XSLoader;
XSLoader::load(
    __PACKAGE__,
    exists $Class::Load::XS::{VERSION}
    ? ${ $Class::Load::XS::{VERSION} }
    : (),
);

1;

# ABSTRACT: XS implementation of parts of Class::Load



=pod

=head1 NAME

Class::Load::XS - XS implementation of parts of Class::Load

=head1 VERSION

version 0.04

=head1 SYNOPSIS

    use Class::Load;

=head1 DESCRIPTION

This module provides an XS implementation for portions of L<Class::Load>. See
L<Class::Load> for API details.

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2012 by Dave Rolsky.

This is free software, licensed under:

  The Artistic License 2.0 (GPL Compatible)

=cut


__END__

