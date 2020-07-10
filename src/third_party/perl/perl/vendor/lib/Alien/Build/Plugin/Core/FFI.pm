package Alien::Build::Plugin::Core::FFI;

use strict;
use warnings;
use Alien::Build::Plugin;

# ABSTRACT: Core FFI plugin
our $VERSION = '1.74'; # VERSION


sub init
{
  my($self, $meta) = @_;

  $meta->default_hook(
    $_ => sub {},
  ) for qw( build_ffi gather_ffi );

  $meta->prop->{destdir_ffi_filter} = '^dynamic';

}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Alien::Build::Plugin::Core::FFI - Core FFI plugin

=head1 VERSION

version 1.74

=head1 SYNOPSIS

 use alienfile;
 # already loaded

=head1 DESCRIPTION

This plugin helps make the build_ffi work.  You should not
need to interact with it directly.

=head1 SEE ALSO

L<Alien::Build>, L<Alien::Base::ModuleBuild>

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
