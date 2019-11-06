package Test::Alien::CanCompile;

use strict;
use warnings;
use Test2::API qw( context );

# ABSTRACT: Skip a test file unless a C compiler is available
our $VERSION = '1.74'; # VERSION


sub skip
{
  require ExtUtils::CBuilder;
  ExtUtils::CBuilder->new->have_compiler ? undef : 'This test requires a compiler.';
}

sub import
{
  my $skip = __PACKAGE__->skip;
  return unless defined $skip;

  my $ctx = context();
  $ctx->plan(0, SKIP => $skip);
  $ctx->release;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test::Alien::CanCompile - Skip a test file unless a C compiler is available

=head1 VERSION

version 1.74

=head1 SYNOPSIS

 use Test::Alien::CanCompile;

=head1 DESCRIPTION

This is just a L<Test2> plugin that requires that a compiler
be available.  Otherwise the test will be skipped.

=head1 SEE ALSO

=over 4

=item L<Test::Alien>

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
