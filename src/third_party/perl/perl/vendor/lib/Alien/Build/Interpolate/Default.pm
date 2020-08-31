package Alien::Build::Interpolate::Default;

use strict;
use warnings;
use base qw( Alien::Build::Interpolate );
use File::chdir;

# ABSTRACT: Default interpolator for Alien::Build
our $VERSION = '1.74'; # VERSION

sub _config
{
  $Config::Config{$_[0]};
}


sub new
{
  my($class) = @_;
  my $self = $class->SUPER::new(@_);


  $self->add_helper( ar => sub { _config 'ar' }, 'Config' );


  $self->add_helper( bison => undef, 'Alien::bison' => '0.17' );


  $self->add_helper( bzip2 => undef, 'Alien::Libbz2' => '0.04' );


  $self->add_helper( cc => sub { _config 'cc' }, 'Config' );


  $self->add_helper( cmake => sub { 'cmake' }, 'Alien::CMake' => '0.07' );


  $self->add_helper( cp => sub { _config 'cp' }, 'Config' );


  $self->add_helper( devnull => sub { $^O eq 'MSWin32' ? 'NUL' : '/dev/null' });


  $self->add_helper( flex => undef, 'Alien::flex' => '0.08' );


  $self->add_helper( gmake => undef, 'Alien::gmake' => '0.11' );


  $self->add_helper( install => sub { 'install' }, 'Alien::MSYS' => '0.07' );


  $self->add_helper( ld => sub { _config 'ld' }, 'Config' );


  $self->add_helper( m4 => undef, 'Alien::m4' => '0.08' );


  $self->add_helper( make => sub { _config 'make' }, 'Config' );


  $self->add_helper( mkdir_deep => sub { $^O eq 'MSWin32' ? 'md' : 'mkdir -p'}, 'Alien::Build' => '1.04' );
  $self->add_helper( make_path  => sub { $^O eq 'MSWin32' ? 'md' : 'mkdir -p'}, 'Alien::Build' => '1.05' );


  $self->add_helper( nasm => undef, 'Alien::nasm' => '0.11' );


  $self->add_helper( patch => undef, 'Alien::patch' => '0.09' );


  $self->add_helper( perl => sub {
      my $perl = Devel::FindPerl::find_perl_interpreter();
      $perl =~ s{\\}{/}g if $^O eq 'MSWin32';
      $perl;
  }, 'Devel::FindPerl' );


  $self->add_helper( pkgconf => undef, 'Alien::pkgconf' => 0.06 );


  $self->add_helper( cwd => sub {
    my $cwd = "$CWD";
    $cwd =~ s{\\}{/}g if $^O eq 'MSWin32';
    $cwd;
  } );


  $self->add_helper( sh => sub { 'sh' }, 'Alien::MSYS' => '0.07' );


  $self->add_helper( rm => sub { _config 'rm' }, 'Config' );



  $self->add_helper( xz => undef, 'Alien::xz' => '0.02' );

  $self;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Alien::Build::Interpolate::Default - Default interpolator for Alien::Build

=head1 VERSION

version 1.74

=head1 CONSTRUCTOR

=head2 new

 my $intr = Alien::Build::Interpolate::Default->new;

=head1 HELPERS

=head2 ar

 %{ar}

The ar command.

=head2 bison

 %{bison}

Requires: L<Alien::bison> 0.17

=head2 bzip2

 %{bzip2}

Requires: L<Alien::Libbz2> 0.04

=head2 cc

 %{cc}

The C Compiler used to build Perl

=head2 cmake

 %{cmake}

Requires: L<Alien::CMake> 0.07

=head2 cp

 %{cp}

The copy command.

=head2 devnull

 %{devnull}

The null device, if available.  On Unix style operating systems this will be C</dev/null> on Windows it is C<NUL>.

=head2 flex

 %{flex}

Requires: L<Alien::flex> 0.08

=head2 gmake

 %{gmake}

Requires: L<Alien::gmake> 0.11

=head2 install

 %{install}

The Unix C<install> command.  On C<MSWin32> this requires L<Alien::MSYS2>.

=head2 ld

 %{ld}

The linker used to build Perl

=head2 m4

 %{m4}

Requires: L<Alien::m4> 0.08

=head2 make

 %{make}

The make program used by Perl.

=head2 make_path

 %{make_path}

Make directory, including all parent directories as needed.  This is usually C<mkdir -p>
on Unix and simply C<md> on windows.

=head2 nasm

 %{nasm}

Requires: L<Alien::nasm> 0.11

=head2 patch

 %{patch}

Requires: L<Alien::patch> 0.09

=head2 perl

 %{perl}

Requires: L<Devel::FindPerl>

=head2 pkgconf

 %{pkgconf}

Requires: L<Alien::pkgconf> 0.06

=head2 cwd

 %{cwd}

=head2 sh

 %{sh}

Unix style command interpreter (/bin/sh).  On MSWin32 this requires L<Alien::MSYS>.

=head2 rm

 %{rm}

The remove command

=head2 xz

 %{xz}

Requires: L<Alien::xz> 0.02

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
