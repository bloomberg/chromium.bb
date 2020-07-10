package Alien::Role;

use strict;
use warnings;

# ABSTRACT: Extend Alien::Base with roles!
our $VERSION = '1.74'; # VERSION


1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Alien::Role - Extend Alien::Base with roles!

=head1 VERSION

version 1.74

=head1 SYNOPSIS

 package Alien::libfoo;
 
 use base qw( Alien::Base );
 use Role::Tiny::With qw( with );
 
 with 'Alien::Role::Dino';
 with 'Alien::Role::Alt';
 
 1;

=head1 DESCRIPTION

The C<Alien::Role> namespace is intended for writing roles that can be
applied to L<Alien::Base> to extend its functionality.  You could of
course write subclasses that extend L<Alien::Base>, but then you have
to either stick with just one subclass or deal with multiple inheritance!
It is recommended that you use L<Role::Tiny> since it can be used on
plain old Perl classes which is good since L<Alien::Base> doesn't use
anything fancy like L<Moose> or L<Moo>.  There are two working examples
that use this technique that are worth checking out in the event you
are interested: L<Alien::Role::Dino> and L<Alien::Role::Alt>.

This class itself doesn't do anything, it just documents the technique.

=head1 SEE ALSO

=over 4

=item L<Alien>

=item L<Alien::Base>

=item L<alienfile>

=item L<Alien::Build>

=item L<Alien::Role::Dino>

=item L<Alien::Role::Alt>

=back

=head1 AUTHOR

Author: Graham Ollis E<lt>plicease@cpan.orgE<gt>

Contributors:

Diab Jerius (DJERIUS)

Roy Storey (KIWIROY)

Ilya Pavlov

David Mertens (run4flat)

Mark Nunberg (mordy, mnunberg)

Christian Walde (Mithaldu)

Brian Wightman (MidLifeXis)

Zaki Mughal (zmughal)

mohawk (mohawk2, ETJ)

Vikas N Kumar (vikasnkumar)

Flavio Poletti (polettix)

Salvador Fandiño (salva)

Gianni Ceccarelli (dakkar)

Pavel Shaydo (zwon, trinitum)

Kang-min Liu (劉康民, gugod)

Nicholas Shipp (nshp)

Juan Julián Merelo Guervós (JJ)

Joel Berger (JBERGER)

Petr Pisar (ppisar)

Lance Wicks (LANCEW)

Ahmad Fatoum (a3f, ATHREEF)

José Joaquín Atria (JJATRIA)

Duke Leto (LETO)

Shoichi Kaji (SKAJI)

Shawn Laffan (SLAFFAN)

Paul Evans (leonerd, PEVANS)

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011-2019 by Graham Ollis.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
